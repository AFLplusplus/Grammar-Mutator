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

#include <cstdint>
#include "list.h"

#include "gtest/gtest.h"

class ListTest : public ::testing::Test {

 protected:
  list_t *list;
  int     _array[3] = {0, 1, 2};

  ListTest() {

    list = nullptr;

  }

  void SetUp() override {

    list = list_create();

    size_t _array_len = sizeof(_array) / sizeof(int);
    for (uint32_t i = 0; i < _array_len; ++i) {

      list_append(list, _array + i);
      EXPECT_EQ(list->tail->data, _array + i);

    }

    EXPECT_EQ(list->size, _array_len);

  }

  void TearDown() override {

    list_free(list);
    list = nullptr;

  }

};

TEST_F(ListTest, ListInsert) {

  size_t list_size = list->size;
  list_insert(list, nullptr);
  EXPECT_EQ(list->head->data, nullptr);
  EXPECT_EQ(list->size, list_size + 1);

}

TEST_F(ListTest, ListAppend) {

  size_t list_size = list->size;
  list_append(list, nullptr);
  EXPECT_EQ(list->tail->data, nullptr);
  EXPECT_EQ(list->size, list_size + 1);

}

TEST_F(ListTest, ListRemove) {

  size_t list_size = list->size;

  EXPECT_TRUE(list_remove(list, _array + 1));
  EXPECT_EQ(list->size, list_size - 1);

  EXPECT_FALSE(list_remove(list, _array + 1));

  EXPECT_TRUE(list_remove(list, _array + 0));
  EXPECT_EQ(list->size, list_size - 2);

}

struct point {

  int x;
  int y;

};

TEST_F(ListTest, ListFreeWithDataFreeFunc) {

  list_t *points = list_create();

  auto point1 = (point *)malloc(sizeof(point));
  auto point2 = (point *)malloc(sizeof(point));

  point1->x = 100;
  point1->y = -10;
  point2->x = 12345;
  point2->y = -12345;

  list_append(points, point1);
  list_append(points, point2);

  list_free_with_data_free_func(points, free);

  // No need to check anything
  // Use Valgrind for memory checking

}

TEST_F(ListTest, ListPopFront) {

  size_t list_size = list->size;
  int *  data = (int *)list_pop_front(list);
  EXPECT_EQ(list->size, list_size - 1);
  EXPECT_EQ(data, _array + 0);
  EXPECT_EQ(*data, _array[0]);

}

TEST_F(ListTest, ListGet) {

  for (uint32_t i = 0; i < list->size; ++i) {

    int *data = (int *)list_get(list, i);
    EXPECT_EQ(data, _array + i);
    EXPECT_EQ(*data, _array[i]);

  }

}

int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();

}
