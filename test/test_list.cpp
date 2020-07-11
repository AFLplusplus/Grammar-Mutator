#include "list.h"

#include "gtest/gtest.h"

class ListTest : public ::testing::Test {
 protected:
  list_t *list;

  ListTest() {
    list = nullptr;
  }

  void SetUp() override {
    list = list_create();
  }

  void TearDown() override {
    list_free(list);
    list = nullptr;
  }
};

TEST_F(ListTest, ListInsert) {
  int _array[] = {0, 1, 2};
  size_t _array_len = sizeof(_array) / sizeof(int);
  for (int i = 0; i < _array_len; ++i) {
    list_insert(list, _array + i);
    EXPECT_EQ(list->head->data, _array + i);
  }
  EXPECT_EQ(list->size, _array_len);
  list_insert(list, nullptr);
  EXPECT_EQ(list->head->data, nullptr);
  EXPECT_EQ(list->size, _array_len + 1);
}

TEST_F(ListTest, ListAppend) {
  int _array[3] = {0, 1, 2};
  size_t _array_len = sizeof(_array) / sizeof(int);
  for (int i = 0; i < _array_len; ++i) {
    list_append(list, _array + i);
    EXPECT_EQ(list->tail->data, _array + i);
  }
  EXPECT_EQ(list->size, _array_len);
  list_append(list, nullptr);
  EXPECT_EQ(list->tail->data, nullptr);
  EXPECT_EQ(list->size, _array_len + 1);
}

TEST_F(ListTest, ListRemove) {
  int _array[3] = {0, 1, 2};
  size_t _array_len = sizeof(_array) / sizeof(int);
  EXPECT_FALSE(list_remove(list, _array + 0));

  for (int i = 0; i < 3; ++i) {
    list_append(list, _array + i);
  }
  EXPECT_TRUE(list_remove(list, _array + 1));
  EXPECT_EQ(list->size, _array_len - 1);
  EXPECT_EQ(list->head->next, list->tail);
  EXPECT_EQ(list->head, list->tail->prev);

  EXPECT_TRUE(list_remove(list, _array + 0));
  EXPECT_EQ(list->size, _array_len - 2);
  EXPECT_EQ(list->head, list->tail);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
