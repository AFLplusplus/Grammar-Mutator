/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Written by Shengtuo Hu

   Copyright 2020 AFLplusplus Project. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   A grammar-based custom mutator written for GSoC '20.

 */

#ifndef GRAMMAR_MUTATOR_GTEST_EXT_H
#define GRAMMAR_MUTATOR_GTEST_EXT_H

#include "gtest/gtest.h"

#define ASSERT_MEMEQ(mem1, mem2, len) \
  ASSERT_EQ(memcmp((void *)(mem1), (void *)(mem2), (int)(len)), 0)
#define EXPECT_MEMEQ(mem1, mem2, len) \
  EXPECT_EQ(memcmp((void *)(mem1), (void *)(mem2), (int)(len)), 0)

#define ASSERT_MEMNE(mem1, mem2, len) \
  ASSERT_NE(memcmp((void *)(mem1), (void *)(mem2), (int)(len)), 0)
#define EXPECT_MEMNE(mem1, mem2, len) \
  EXPECT_NE(memcmp((void *)(mem1), (void *)(mem2), (int)(len)), 0)

#endif  // GRAMMAR_MUTATOR_GTEST_EXT_H
