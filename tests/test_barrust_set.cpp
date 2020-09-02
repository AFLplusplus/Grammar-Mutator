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

#include "set.h"

#include "gtest/gtest.h"

/**
 * Test cases are imported from the original Github repo:
 * https://github.com/barrust/set/blob/master/tests/set_test.c
 */
#define KEY_LEN 25
void initialize_set(simple_set *set, int start, int elements, int itter,
                    int TEST) {

  int  i;
  char key[KEY_LEN] = {0};
  for (i = start; i < elements; i += itter) {

    sprintf(key, "%d", i);
    int res = set_add(set, key);
    ASSERT_EQ(res, TEST);

  }

}

class BarrustSet : public ::testing::Test {

 protected:
  simple_set set_a = {.nodes = nullptr,
                      .number_nodes = 0,
                      .used_nodes = 0,
                      .hash_function = nullptr};
  simple_set set_b = {.nodes = nullptr,
                      .number_nodes = 0,
                      .used_nodes = 0,
                      .hash_function = nullptr};
  simple_set set_c = {.nodes = nullptr,
                      .number_nodes = 0,
                      .used_nodes = 0,
                      .hash_function = nullptr};

  uint64_t elements = 1000;

  BarrustSet() = default;

  void SetUp() override {

    set_init(&set_a);
    initialize_set(&set_a, 0, elements, 1, SET_TRUE);
    ASSERT_EQ(set_length(&set_a), elements);

    set_init(&set_b);
    initialize_set(&set_b, 0, elements / 2, 1, SET_TRUE);
    ASSERT_EQ(set_length(&set_b), elements / 2);

    set_init(&set_c);

  }

  void TearDown() override {

    set_destroy(&set_a);
    set_destroy(&set_b);
    set_destroy(&set_c);

  }

};

TEST_F(BarrustSet, SetAdd) {

  // double insertion
  initialize_set(&set_a, 0, elements / 2, 1, SET_ALREADY_PRESENT);
  ASSERT_EQ(set_length(&set_a), elements);

}

TEST_F(BarrustSet, SetToArray) {

  uint64_t ui;
  char **  keys = set_to_array(&set_a, &ui);
  ASSERT_EQ(set_length(&set_a), ui);

  // free the keys memory
  for (uint64_t i = 0; i < ui; ++i) {

    free(keys[i]);

  }

  free(keys);

}

TEST_F(BarrustSet, SetContains) {

  char key[KEY_LEN] = {0};
  for (uint64_t i = 0; i < elements; ++i) {

    sprintf(key, "%" PRIu64, i);
    ASSERT_EQ(set_contains(&set_a, key), SET_TRUE);

  }

  // non-existing keys
  for (uint64_t i = elements; i < elements * 2; ++i) {

    sprintf(key, "%" PRIu64, i);
    ASSERT_EQ(set_contains(&set_a, key), SET_FALSE);

  }

}

TEST_F(BarrustSet, SetRemove) {

  char key[KEY_LEN] = {0};
  for (uint64_t i = elements / 2; i < elements; ++i) {

    sprintf(key, "%" PRIu64, i);
    ASSERT_EQ(set_remove(&set_a, key), SET_TRUE);

  }

  ASSERT_EQ(set_length(&set_a), elements / 2);

  // check removed keys
  for (uint64_t i = 0; i < elements; ++i) {

    sprintf(key, "%" PRIu64, i);
    if (i >= elements / 2) {

      ASSERT_EQ(set_contains(&set_a, key), SET_FALSE);

    } else {

      ASSERT_EQ(set_contains(&set_a, key), SET_TRUE);

    }

  }

}

TEST_F(BarrustSet, SetClear) {

  EXPECT_EQ(set_clear(&set_a), SET_TRUE);
  ASSERT_EQ(set_length(&set_a), 0);
  for (uint64_t i = 0; i < set_a.number_nodes; ++i) {

    ASSERT_EQ(set_a.nodes[i], nullptr);

  }

}

TEST_F(BarrustSet, SetIsSubset) {

  // truth: B ⊆ A
  EXPECT_EQ(set_is_subset(&set_a, &set_b), SET_FALSE);
  EXPECT_EQ(set_is_subset(&set_b, &set_a), SET_TRUE);

  // A ⊆ A
  EXPECT_EQ(set_is_subset(&set_a, &set_a), SET_TRUE);
  // B ⊆ B
  EXPECT_EQ(set_is_subset(&set_b, &set_b), SET_TRUE);

}

TEST_F(BarrustSet, SetIsSubsetStrict) {

  // truth: B ⊂ A
  EXPECT_EQ(set_is_subset_strict(&set_a, &set_b), SET_FALSE);
  EXPECT_EQ(set_is_subset_strict(&set_b, &set_a), SET_TRUE);

  // A ⊂ A
  EXPECT_EQ(set_is_subset_strict(&set_a, &set_a), SET_FALSE);
  // B ⊂ B
  EXPECT_EQ(set_is_subset_strict(&set_b, &set_b), SET_FALSE);

}

TEST_F(BarrustSet, SetIsSuperset) {

  // truth: A ⊇ B
  EXPECT_EQ(set_is_superset(&set_a, &set_b), SET_TRUE);
  EXPECT_EQ(set_is_superset(&set_b, &set_a), SET_FALSE);

  // A ⊇ A
  EXPECT_EQ(set_is_superset(&set_a, &set_a), SET_TRUE);
  // B ⊇ B
  EXPECT_EQ(set_is_superset(&set_b, &set_b), SET_TRUE);

}

TEST_F(BarrustSet, SetIsSupersetStrict) {

  // truth: A ⊃ B
  EXPECT_EQ(set_is_superset_strict(&set_a, &set_b), SET_TRUE);
  EXPECT_EQ(set_is_superset_strict(&set_b, &set_a), SET_FALSE);

  // A ⊃ A
  EXPECT_EQ(set_is_superset_strict(&set_a, &set_a), SET_FALSE);
  // B ⊃ B
  EXPECT_EQ(set_is_superset_strict(&set_b, &set_b), SET_FALSE);

}

TEST_F(BarrustSet, SetIntersection) {

  ASSERT_EQ(set_intersection(&set_c, &set_a, &set_b), SET_TRUE);
  ASSERT_EQ(set_length(&set_c), elements / 2);

  char key[KEY_LEN] = {0};
  for (uint64_t i = 0; i < elements / 2; ++i) {

    sprintf(key, "%" PRIu64, i);
    ASSERT_EQ(set_contains(&set_c, key), SET_TRUE);

  }

  for (uint64_t i = elements / 2; i < elements; ++i) {

    sprintf(key, "%" PRIu64, i);
    ASSERT_EQ(set_contains(&set_c, key), SET_FALSE);

  }

  for (uint64_t i = elements; i < elements * 2; ++i) {

    sprintf(key, "%" PRIu64, i);
    ASSERT_EQ(set_contains(&set_c, key), SET_FALSE);

  }

}

TEST_F(BarrustSet, SetDifference) {

  // C = A \ B
  ASSERT_EQ(set_difference(&set_c, &set_a, &set_b), SET_TRUE);
  ASSERT_EQ(set_length(&set_c), elements / 2);

  char key[KEY_LEN] = {0};
  for (uint64_t i = 0; i < elements / 2; ++i) {

    sprintf(key, "%" PRIu64, i);
    ASSERT_EQ(set_contains(&set_c, key), SET_FALSE);

  }

  for (uint64_t i = elements / 2; i < elements; ++i) {

    sprintf(key, "%" PRIu64, i);
    ASSERT_EQ(set_contains(&set_c, key), SET_TRUE);

  }

  for (uint64_t i = elements; i < elements * 2; ++i) {

    sprintf(key, "%" PRIu64, i);
    ASSERT_EQ(set_contains(&set_c, key), SET_FALSE);

  }

}

TEST_F(BarrustSet, SetSymmetricDifference) {

  ASSERT_EQ(set_destroy(&set_b), SET_TRUE);
  ASSERT_EQ(set_init(&set_b), SET_TRUE);
  initialize_set(&set_b, elements / 2, elements * 2, 1, SET_TRUE);

  // C = A △ B
  ASSERT_EQ(set_symmetric_difference(&set_c, &set_a, &set_b), SET_TRUE);
  ASSERT_EQ(set_length(&set_c), (elements * 2) - (elements / 2));

  char key[KEY_LEN] = {0};
  for (uint64_t i = 0; i < elements / 2; ++i) {

    sprintf(key, "%" PRIu64, i);
    ASSERT_EQ(set_contains(&set_c, key), SET_TRUE);

  }

  for (uint64_t i = elements / 2; i < elements; ++i) {

    sprintf(key, "%" PRIu64, i);
    ASSERT_EQ(set_contains(&set_c, key), SET_FALSE);

  }

  for (uint64_t i = elements; i < elements * 2; ++i) {

    sprintf(key, "%" PRIu64, i);
    ASSERT_EQ(set_contains(&set_c, key), SET_TRUE);

  }

}

TEST_F(BarrustSet, SetUnion) {

  ASSERT_EQ(set_destroy(&set_b), SET_TRUE);
  ASSERT_EQ(set_init(&set_b), SET_TRUE);
  initialize_set(&set_b, elements / 2, elements * 2, 1, SET_TRUE);

  // C = A ∪ B
  ASSERT_EQ(set_union(&set_c, &set_a, &set_b), SET_TRUE);
  ASSERT_EQ(set_length(&set_c), elements * 2);

  char key[KEY_LEN] = {0};
  for (uint64_t i = 0; i < elements * 2; ++i) {

    sprintf(key, "%" PRIu64, i);
    ASSERT_EQ(set_contains(&set_c, key), SET_TRUE);

  }

}

TEST_F(BarrustSet, SetCompare) {

  set_clear(&set_b);
  initialize_set(&set_b, 1, elements + 1, 1, SET_TRUE);
  initialize_set(&set_c, 0, elements * 2, 1, SET_TRUE);

  EXPECT_EQ(set_cmp(&set_c, &set_a), SET_LEFT_GREATER);
  EXPECT_EQ(set_cmp(&set_a, &set_c), SET_RIGHT_GREATER);
  EXPECT_EQ(set_cmp(&set_a, &set_a), SET_EQUAL);
  EXPECT_EQ(set_cmp(&set_a, &set_b), SET_UNEQUAL);

}

int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();

}

