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

#include "map.h"
#include "list.h"

#include "gtest/gtest.h"

typedef map_t(list_t *) list_map_t;

class RxiMapTest : public ::testing::Test {

 protected:
  list_map_t list_map;

  RxiMapTest() = default;

  void SetUp() override {

    map_init(&list_map);

  }

  void TearDown() override {

    const char *key;
    map_iter_t  iter = map_iter(&list_map);
    while ((key = map_next(&list_map, &iter))) {

      list_free(*map_get(&list_map, key));

    }

    map_deinit(&list_map);

  }

};

TEST_F(RxiMapTest, MapGet) {

  // non-existing key
  list_t **p_list = map_get(&list_map, "key1");
  EXPECT_EQ(p_list, nullptr);

  // add a list
  list_t *list = list_create();
  map_set(&list_map, "key1", list);

  p_list = map_get(&list_map, "key1");
  EXPECT_NE(p_list, nullptr);
  EXPECT_EQ(*p_list, list);

}

TEST_F(RxiMapTest, MapSet) {

  list_t *list = list_create();
  EXPECT_EQ(map_set(&list_map, "key1", list), 0);

}

TEST_F(RxiMapTest, MapRemove) {

  // should not fail
  map_remove(&list_map, "key1");

  // add a list
  list_t *list = list_create();
  map_set(&list_map, "key1", list);
  list_t **p_list = map_get(&list_map, "key1");
  EXPECT_NE(p_list, nullptr);
  EXPECT_EQ(*p_list, list);

  map_remove(&list_map, "key1");

  // non-existing key
  p_list = map_get(&list_map, "key1");
  EXPECT_EQ(p_list, nullptr);

  list_free(list);

}

int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();

}
