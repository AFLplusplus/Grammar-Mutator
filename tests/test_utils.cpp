/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   A grammar-based custom mutator written for GSoC '20.

 */

#include "utils.h"

#include "gtest/gtest.h"

TEST(UtilsTest, HashBytesIsStableAndNotZero) {

  EXPECT_EQ(hash_bytes("abc", 3), hash_bytes("abc", 3));
  EXPECT_NE(hash_bytes("abc", 3), hash_bytes("abd", 3));
  EXPECT_NE(hash_bytes("abc", 3), hash_bytes("abc", 2));
  EXPECT_NE(hash_bytes("", 0), 0u);

}

TEST(UtilsTest, HashSetAddsEachHashOnce) {

  hash_set_t *set = hash_set_create();
  ASSERT_NE(set, nullptr);
  EXPECT_TRUE(hash_set_add(set, 1));
  EXPECT_FALSE(hash_set_add(set, 1));
  EXPECT_TRUE(hash_set_add(set, 2));
  EXPECT_TRUE(hash_set_add(set, 0));
  EXPECT_FALSE(hash_set_add(set, 0));
  EXPECT_EQ(hash_set_size(set), 3u);
  hash_set_free(set);

}

TEST(UtilsTest, HashSetGrows) {

  hash_set_t *set = hash_set_create();
  ASSERT_NE(set, nullptr);
  for (uint64_t i = 1; i <= 10000; ++i)
    EXPECT_TRUE(hash_set_add(set, i * 0x9e3779b97f4a7c15ULL));
  for (uint64_t i = 1; i <= 10000; ++i)
    EXPECT_FALSE(hash_set_add(set, i * 0x9e3779b97f4a7c15ULL));
  EXPECT_EQ(hash_set_size(set), 10000u);
  hash_set_free(set);

}
