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

namespace llvm {
TEST(PagedVectorTest, FunctionalityTest) {
  PagedVector<int, 10> V;

  // Next ten numbers are 10..19
  V.expand(2);
  V.expand(10);
  V.expand(20);
  V.expand(30);
  EXPECT_EQ(V.materialised().size(), 0ULL);

  EXPECT_EQ(V.size(), 30ULL);
  for (int I = 0; I < 10; ++I) {
    V[I] = I;
  }
  for (int I = 0; I < 10; ++I) {
    EXPECT_EQ(V[I], I);
  }
  EXPECT_EQ(V.materialised().size(), 10ULL);
  for (int I = 20; I < 30; ++I) {
    V[I] = I;
  }
  for (int I = 20; I < 30; ++I) {
    EXPECT_EQ(V[I], I);
  }
  EXPECT_EQ(V.materialised().size(), 20ULL);

  for (int I = 10; I < 20; ++I) {
    V[I] = I;
  }
  for (int I = 10; I < 20; ++I) {
    EXPECT_EQ(V[I], I);
  }
  EXPECT_EQ(V.materialised().size(), 30ULL);
  V.expand(35);
  EXPECT_EQ(V.materialised().size(), 30ULL);
  for (int I = 30; I < 35; ++I) {
    V[I] = I;
  }
  EXPECT_EQ(V.materialised().size(), 40ULL);
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
  }
  for (int I = 37; I < 40; ++I) {
    EXPECT_EQ(V[I], 0);
  }
  V.expand(50);
  EXPECT_EQ(V.capacity(), 50ULL);
  EXPECT_EQ(V.size(), 50ULL);
  EXPECT_EQ(V[40], 40);
  V.expand(50ULL);
}
} // namespace llvm
