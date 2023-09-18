//===- llvm/unittest/ADT/PagedVectorTest.cpp ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// PagedVector unit tests.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/PagedVector.h"
#include "gtest/gtest.h"
#include <iterator>

namespace llvm {
TEST(PagedVectorTest, EmptyTest) {
  PagedVector<int, 10> V;
  EXPECT_EQ(V.empty(), true);
  EXPECT_EQ(V.size(), 0ULL);
  EXPECT_EQ(V.capacity(), 0ULL);
  EXPECT_EQ(V.materialisedBegin().getIndex(), 0ULL);
  EXPECT_EQ(V.materialisedEnd().getIndex(), 0ULL);
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 0ULL);
}

TEST(PagedVectorTest, ExpandTest) {
  PagedVector<int, 10> V;
  V.expand(2);
  EXPECT_EQ(V.empty(), false);
  EXPECT_EQ(V.size(), 2ULL);
  EXPECT_EQ(V.capacity(), 10ULL);
  EXPECT_EQ(V.materialisedBegin().getIndex(), 2ULL);
  EXPECT_EQ(V.materialisedEnd().getIndex(), 2ULL);
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 0ULL);
}

TEST(PagedVectorTest, FullPageFillingTest) {
  PagedVector<int, 10> V;
  V.expand(10);
  EXPECT_EQ(V.empty(), false);
  EXPECT_EQ(V.size(), 10ULL);
  EXPECT_EQ(V.capacity(), 10ULL);
  for (int I = 0; I < 10; ++I) {
    V[I] = I;
  }
  EXPECT_EQ(V.empty(), false);
  EXPECT_EQ(V.size(), 10ULL);
  EXPECT_EQ(V.capacity(), 10ULL);
  EXPECT_EQ(V.materialisedBegin().getIndex(), 0ULL);
  EXPECT_EQ(V.materialisedEnd().getIndex(), 10ULL);
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 10ULL);
  for (int I = 0; I < 10; ++I) {
    EXPECT_EQ(V[I], I);
  }
}

TEST(PagedVectorTest, HalfPageFillingTest) {
  PagedVector<int, 10> V;
  V.expand(5);
  EXPECT_EQ(V.empty(), false);
  EXPECT_EQ(V.size(), 5ULL);
  EXPECT_EQ(V.capacity(), 10ULL);
  for (int I = 0; I < 5; ++I) {
    V[I] = I;
  }
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 5ULL);
  for (int I = 0; I < 5; ++I) {
    EXPECT_EQ(V[I], I);
  }
}

TEST(PagedVectorTest, FillFullMultiPageTest) {
  PagedVector<int, 10> V;
  V.expand(20);
  EXPECT_EQ(V.empty(), false);
  EXPECT_EQ(V.size(), 20ULL);
  EXPECT_EQ(V.capacity(), 20ULL);
  for (int I = 0; I < 20; ++I) {
    V[I] = I;
  }
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 20ULL);
}

TEST(PagedVectorTest, FillHalfMultiPageTest) {
  PagedVector<int, 10> V;
  V.expand(20);
  EXPECT_EQ(V.empty(), false);
  EXPECT_EQ(V.size(), 20ULL);
  EXPECT_EQ(V.capacity(), 20ULL);
  for (int I = 0; I < 5; ++I) {
    V[I] = I;
  }
  for (int I = 10; I < 15; ++I) {
    V[I] = I;
  }
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 20ULL);
  for (int I = 0; I < 5; ++I) {
    EXPECT_EQ(V[I], I);
  }
  for (int I = 10; I < 15; ++I) {
    EXPECT_EQ(V[I], I);
  }
}

TEST(PagedVectorTest, FillLastMultiPageTest) {
  PagedVector<int, 10> V;
  V.expand(20);
  EXPECT_EQ(V.empty(), false);
  EXPECT_EQ(V.size(), 20ULL);
  EXPECT_EQ(V.capacity(), 20ULL);
  for (int I = 10; I < 15; ++I) {
    V[I] = I;
  }
  for (int I = 10; I < 15; ++I) {
    EXPECT_EQ(V[I], I);
  }

  // Since we fill the last page only, the materialised vector
  // should contain only the last page.
  int J = 10;
  for (auto MI = V.materialisedBegin(), ME = V.materialisedEnd(); MI != ME;
       ++MI) {
    if (J < 15) {
      EXPECT_EQ(*MI, J);
    } else {
      EXPECT_EQ(*MI, 0);
    }
    ++J;
  }
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 10ULL);
}

// Filling the first element of all the pages
// will allocate all of them
TEST(PagedVectorTest, FillSparseMultiPageTest) {
  PagedVector<int, 10> V;
  V.expand(100);
  EXPECT_EQ(V.empty(), false);
  EXPECT_EQ(V.size(), 100ULL);
  EXPECT_EQ(V.capacity(), 100ULL);
  for (int I = 0; I < 10; ++I) {
    V[I * 10] = I;
  }
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 100ULL);
  for (int I = 0; I < 100; ++I) {
    if (I % 10 == 0) {
      EXPECT_EQ(V[I], I / 10);
    } else {
      EXPECT_EQ(V[I], 0);
    }
  }
}

struct TestHelper {
  int A = -1;
};

TEST(PagedVectorTest, FillNonTrivialConstructor) {
  PagedVector<TestHelper, 10> V;
  V.expand(10);
  EXPECT_EQ(V.empty(), false);
  EXPECT_EQ(V.size(), 10ULL);
  EXPECT_EQ(V.capacity(), 10ULL);
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 0ULL);
  for (int I = 0; I < 10; ++I) {
    EXPECT_EQ(V[I].A, -1);
  }
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 10ULL);
}

TEST(PagedVectorTest, FunctionalityTest) {
  PagedVector<int, 10> V;
  EXPECT_EQ(V.empty(), true);

  // Next ten numbers are 10..19
  V.expand(2);
  EXPECT_EQ(V.empty(), false);
  V.expand(10);
  V.expand(20);
  V.expand(30);
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 0ULL);

  EXPECT_EQ(V.size(), 30ULL);
  for (int I = 0; I < 10; ++I) {
    V[I] = I;
  }
  for (int I = 0; I < 10; ++I) {
    EXPECT_EQ(V[I], I);
  }
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 10ULL);
  for (int I = 20; I < 30; ++I) {
    V[I] = I;
  }
  for (int I = 20; I < 30; ++I) {
    EXPECT_EQ(V[I], I);
  }
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 20ULL);

  for (int I = 10; I < 20; ++I) {
    V[I] = I;
  }
  for (int I = 10; I < 20; ++I) {
    EXPECT_EQ(V[I], I);
  }
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 30ULL);
  V.expand(35);
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 30ULL);
  for (int I = 30; I < 35; ++I) {
    V[I] = I;
  }
  EXPECT_EQ(std::distance(V.materialisedBegin(), V.materialisedEnd()), 35ULL);
  EXPECT_EQ(V.size(), 35ULL);
  EXPECT_EQ(V.capacity(), 40ULL);
  V.expand(37);
  for (int I = 30; I < 37; ++I) {
    V[I] = I;
  }
  EXPECT_EQ(V.size(), 37ULL);
  EXPECT_EQ(V.capacity(), 40ULL);
  for (int I = 0; I < 37; ++I) {
    EXPECT_EQ(V[I], I);
  }

  V.expand(41);
  V[40] = 40;
  EXPECT_EQ(V.size(), 41ULL);
  EXPECT_EQ(V.capacity(), 50ULL);
  for (int I = 0; I < 36; ++I) {
    EXPECT_EQ(V[I], I);
    EXPECT_EQ(V.at(I), I);
  }
  for (int I = 37; I < 40; ++I) {
    EXPECT_EQ(V[I], 0);
    EXPECT_EQ(V.at(I), 0);
  }
  V.expand(50);
  EXPECT_EQ(V.capacity(), 50ULL);
  EXPECT_EQ(V.size(), 50ULL);
  EXPECT_EQ(V[40], 40);
  EXPECT_EQ(V.at(40), 40);
  V.expand(50ULL);
  V.clear();
  EXPECT_EQ(V.size(), 0ULL);
  EXPECT_EQ(V.capacity(), 0ULL);
}
} // namespace llvm
