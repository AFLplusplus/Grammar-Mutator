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
