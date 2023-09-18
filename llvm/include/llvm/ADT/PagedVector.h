//===- llvm/ADT/PagedVector.h - 'Lazyly allocated' vectors --------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the PagedVector class.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_ADT_PAGEDVECTOR_H
#define LLVM_ADT_PAGEDVECTOR_H

#include "llvm/Support/Allocator.h"
#include <cassert>
#include <iostream>
#include <vector>

namespace llvm {
// A vector that allocates memory in pages.
// Order is kept, but memory is allocated only when one element of the page is
// accessed. This introduces a level of indirection, but it is useful when you
// have a sparsely initialised vector where the full size is allocated upfront
// with the default constructor and elements are initialised later, on first
// access.
//
// Notice that this does not have iterators, because if you
// have iterators it probably means you are going to touch
// all the memory in any case, so better use a std::vector in
// the first place.
//
// Pages are allocated in SLAB_SIZE chunks, using the BumpPtrAllocator.
template <typename T, std::size_t PAGE_SIZE = 1024 / sizeof(T)>
class PagedVector {
  static_assert(PAGE_SIZE > 0, "PAGE_SIZE must be greater than 0. Most likely "
                               "you want it to be greater than 16.");
  // The actual number of element in the vector which can be accessed.
  std::size_t Size = 0;

  // The position of the initial element of the page in the Data vector.
  // Pages are allocated contiguously in the Data vector.
  mutable std::vector<T *> PageToDataIdx;
  // Actual page data. All the page elements are added to this vector on the
  // first access of any of the elements of the page. Elements default
  // constructed and elements of the page are stored contiguously. The order of
  // the elements however depends on the order of access of the pages.
  uintptr_t Allocator = 0;

  constexpr static T *invalidPage() { return reinterpret_cast<T *>(SIZE_MAX); }

public:
  // Default constructor. We build our own allocator.
  PagedVector()
      : Allocator(reinterpret_cast<uintptr_t>(new BumpPtrAllocator) | 0x1) {}
  PagedVector(BumpPtrAllocator *A)
      : Allocator(reinterpret_cast<uintptr_t>(A)) {}

  ~PagedVector() {
    // If we own the allocator, delete it.
    if (Allocator & 0x1) {
      delete getAllocator();
    }
  }

  // Get the allocator.
  BumpPtrAllocator *getAllocator() const {
    return reinterpret_cast<BumpPtrAllocator *>(Allocator & ~0x1);
  }
  // Lookup an element at position Index.
  T &operator[](std::size_t Index) const { return at(Index); }

  // Lookup an element at position i.
  // If the associated page is not filled, it will be filled with default
  // constructed elements. If the associated page is filled, return the element.
  T &at(std::size_t Index) const {
    assert(Index < Size);
    assert(Index / PAGE_SIZE < PageToDataIdx.size());
    auto *&PagePtr = PageToDataIdx[Index / PAGE_SIZE];
    // If the page was not yet allocated, allocate it.
    if (PagePtr == invalidPage()) {
      PagePtr = getAllocator()->template Allocate<T>(PAGE_SIZE);
      // We need to invoke the default constructor on all the elements of the
      // page.
      for (std::size_t I = 0; I < PAGE_SIZE; ++I) {
        new (PagePtr + I) T();
      }
    }
    // Dereference the element in the page.
    return *((Index % PAGE_SIZE) + PagePtr);
  }

  // Return the capacity of the vector. I.e. the maximum size it can be expanded
  // to with the expand method without allocating more pages.
  std::size_t capacity() const { return PageToDataIdx.size() * PAGE_SIZE; }

  // Return the size of the vector. I.e. the maximum index that can be
  // accessed, i.e. the maximum value which was used as argument of the
  // expand method.
  std::size_t size() const { return Size; }

  // Expands the vector to the given NewSize number of elements.
  // If the vector was smaller, allocates new pages as needed.
  // It should be called only with NewSize >= Size.
  void expand(std::size_t NewSize) {
    // You cannot shrink the vector, otherwise
    // one would have to invalidate contents which is expensive and
    // while giving the false hope that the resize is cheap.
    if (NewSize <= Size) {
      return;
    }
    // If the capacity is enough, just update the size and continue
    // with the currently allocated pages.
    if (NewSize <= capacity()) {
      Size = NewSize;
      return;
    }
    // The number of pages to allocate. The Remainder is calculated
    // for the case in which the NewSize is not a multiple of PAGE_SIZE.
    // In that case we need one more page.
    auto Pages = NewSize / PAGE_SIZE;
    auto Remainder = NewSize % PAGE_SIZE;
    if (Remainder) {
      Pages += 1;
    }
    assert(Pages > PageToDataIdx.size());
    // We use invalidPage() to indicate that a page has not been allocated yet.
    // This cannot be 0, because 0 is a valid page id.
    // We use invalidPage() instead of a separate bool to avoid wasting space.
    PageToDataIdx.resize(Pages, invalidPage());
    Size = NewSize;
  }

  // Return true if the vector is empty
  bool empty() const { return Size == 0; }

  /// Clear the vector, i.e. clear the allocated pages, the whole page
  /// lookup index and reset the size.
  void clear() {
    Size = 0;
    // If we own the allocator, simply reset it, otherwise we
    // deallocate the pages one by one.
    if (Allocator & 0x1) {
      getAllocator()->Reset();
    } else {
      for (auto *Page : PageToDataIdx) {
        getAllocator()->Deallocate(Page);
      }
    }
    PageToDataIdx.clear();
  }

  // Iterator on all the elements of the vector
  // which have actually being constructed.
  class MaterialisedIterator {
    PagedVector const *PV;
    size_t ElementIdx;

  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T *;
    using reference = T &;

    MaterialisedIterator(PagedVector const *PV, size_t ElementIdx)
        : PV(PV), ElementIdx(ElementIdx) {}

    // When incrementing the iterator, we skip the elements which have not
    // been materialised yet.
    MaterialisedIterator &operator++() {
      while (ElementIdx < PV->Size) {
        ++ElementIdx;
        if (PV->PageToDataIdx[ElementIdx / PAGE_SIZE] != invalidPage()) {
          return *this;
        }
      }
      return *this;
    }
    // Post increment operator.
    MaterialisedIterator operator++(int) {
      auto Copy = *this;
      ++*this;
      return Copy;
    }

    std::ptrdiff_t operator-(MaterialisedIterator const &Other) const {
      assert(PV == Other.PV);
      // If they are on the same table we can just subtract the indices.
      // Otherwise we have to iterate over the pages to find the difference.
      // If a page is invalid, we skip it.
      if (PV == Other.PV) {
        return ElementIdx - Other.ElementIdx;
      }

      auto ElementMin = std::min(ElementIdx, Other.ElementIdx);
      auto ElementMax = std::max(ElementIdx, Other.ElementIdx);
      auto PageMin = ElementMin / PAGE_SIZE;
      auto PageMax = ElementMax / PAGE_SIZE;

      auto Count = 0ULL;
      for (auto PageIdx = PageMin; PageIdx < PageMax; ++PageIdx) {
        if (PV->PageToDataIdx[PageIdx] == invalidPage()) {
          continue;
        }
        Count += PAGE_SIZE;
      }
      Count += ElementMax % PAGE_SIZE;
      Count += PAGE_SIZE - ElementMin % PAGE_SIZE;

      return Count;
    }

    // When dereferencing the iterator, we materialise the page if needed.
    T const &operator*() const {
      assert(ElementIdx < PV->Size);
      assert(PV->PageToDataIdx[ElementIdx / PAGE_SIZE] != invalidPage());
      return *((ElementIdx % PAGE_SIZE) +
               PV->PageToDataIdx[ElementIdx / PAGE_SIZE]);
    }

    // Equality operator.
    bool operator==(MaterialisedIterator const &Other) const {
      // Iterators of two different vectors are never equal.
      if (PV != Other.PV) {
        return false;
      }
      // Any iterator for an empty vector is equal to any other iterator.
      if (PV->empty()) {
        return true;
      }
      // Get the pages of the two iterators. If between the two pages there
      // are no valid pages, we can condider the iterators equal.
      auto PageMin = std::min(ElementIdx, Other.ElementIdx) / PAGE_SIZE;
      auto PageMax = std::max(ElementIdx, Other.ElementIdx) / PAGE_SIZE;
      // If the two pages are past the end, the iterators are equal.
      if (PageMin >= PV->PageToDataIdx.size()) {
        return true;
      }
      // If only the last page is past the end, the iterators are equal if
      // all the pages up to the end are invalid.
      if (PageMax >= PV->PageToDataIdx.size()) {
        for (auto PageIdx = PageMin; PageIdx < PV->PageToDataIdx.size();
             ++PageIdx) {
          if (PV->PageToDataIdx[PageIdx] != invalidPage()) {
            return false;
          }
        }
        return true;
      }

      auto *Page1 = PV->PageToDataIdx[PageMin];
      auto *Page2 = PV->PageToDataIdx[PageMax];
      if (Page1 == invalidPage() && Page2 == invalidPage()) {
        return true;
      }
      // If the two pages are the same, the iterators are equal if they point
      // to the same element.
      if (PageMin == PageMax) {
        return ElementIdx == Other.ElementIdx;
      }
      // If the two pages are different, the iterators are equal if all the
      // pages between them are invalid.
      for (auto PageIdx = PageMin; PageIdx < PageMax; ++PageIdx) {
        if (PV->PageToDataIdx[PageIdx] != invalidPage()) {
          return false;
        }
      }
      return true;
    }

    bool operator!=(MaterialisedIterator const &Other) const {
      return (*this == Other) == false;
    }

    [[nodiscard]] size_t getIndex() const { return ElementIdx; }
  };

  // Iterators over the materialised elements of the vector.
  // This includes all the elements belonging to allocated pages,
  // even if they have not been accessed yet. It's enough to access
  // one element of a page to materialise all the elements of the page.
  MaterialisedIterator materialisedBegin() const {
    // Look for the first valid page
    auto ElementIdx = 0ULL;
    while (ElementIdx < Size) {
      if (PageToDataIdx[ElementIdx / PAGE_SIZE] != invalidPage()) {
        break;
      }
      ++ElementIdx;
    }
    return MaterialisedIterator(this, ElementIdx);
  }

  MaterialisedIterator materialisedEnd() const {
    return MaterialisedIterator(this, Size);
  }
};
} // namespace llvm
#endif // LLVM_ADT_PAGEDVECTOR_H
