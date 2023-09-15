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

#include <vector>

// A vector that allocates memory in pages.
// Order is kept, but memory is allocated only when one element of the page is
// accessed.
// Notice that this does not have iterators, because if you
// have iterators it probably means you are going to touch
// all the memory in any case, so better use a std::vector in
// the first place.
template <typename T, int PAGE_SIZE = 1024 / sizeof(T)> class PagedVector {
  size_t Size = 0;
  // Index of where to find a given page in the data
  mutable std::vector<int> Lookup;
  // Actual page data
  mutable std::vector<T> Data;

public:
  // Lookup an element at position i.
  // If the associated page is not filled, it will be filled with default
  // constructed elements. If the associated page is filled, return the element.
  T &operator[](int Index) const { return at(Index); }

  T &at(int Index) const {
    auto &PageId = Lookup[Index / PAGE_SIZE];
    // If the range is not filled, fill it
    if (PageId == -1) {
      int OldSize = Data.size();
      PageId = OldSize / PAGE_SIZE;
      // Allocate the memory
      Data.resize(OldSize + PAGE_SIZE);
      // Fill the whole capacity with empty elements
      for (int I = 0; I < PAGE_SIZE; ++I) {
        Data[I + OldSize] = T();
      }
    }
    // Return the element
    return Data[Index % PAGE_SIZE + PAGE_SIZE * PageId];
  }

  // Return the size of the vector
  size_t capacity() const { return Lookup.size() * PAGE_SIZE; }

  size_t size() const { return Size; }

  // Expands the vector to the given size.
  // If the vector is already bigger, does nothing.
  void expand(size_t NewSize) {
    // You cannot shrink the vector, otherwise
    // you would have to invalidate
    assert(NewSize >= Size);
    if (NewSize <= Size) {
      return;
    }
    if (NewSize <= capacity()) {
      Size = NewSize;
      return;
    }
    auto Pages = NewSize / PAGE_SIZE;
    auto Remainder = NewSize % PAGE_SIZE;
    if (Remainder) {
      Pages += 1;
    }
    assert(Pages > Lookup.size());
    Lookup.resize(Pages, -1);
    Size = NewSize;
  }

  // Return true if the vector is empty
  bool empty() const { return Size == 0; }

  /// Clear the vector
  void clear() {
    Size = 0;
    Lookup.clear();
    Data.clear();
  }

  std::vector<T> const &materialised() const { return Data; }
};

#endif // LLVM_ADT_PAGEDVECTOR_H
