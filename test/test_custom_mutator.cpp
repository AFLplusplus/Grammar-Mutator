#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include <string>

#include "custom_mutator.h"
#include "tree.h"

#include "gtest/gtest.h"

using namespace std;

struct custom_mutator {
  my_mutator_t *data;
};

static void dump_test_case(uint8_t *buf, size_t buf_size) {
  fprintf(stderr, "%.*s\n", (int)buf_size, buf);
}

static int num = 100;

extern map<string, tree_t *> trees;

class CustomMutatorTest : public ::testing::Test {
 protected:
  afl_t *                afl = nullptr;
  struct custom_mutator *mutator = nullptr;

  CustomMutatorTest() = default;

  ~CustomMutatorTest() override = default;

  void SetUp() override {
    mutator = (struct custom_mutator *)calloc(1, sizeof(struct custom_mutator));
    ASSERT_NE(mutator, nullptr);

    // Initialize the custom mutator
    mutator->data = afl_custom_init(afl, 0);  // fixed random seed
    ASSERT_NE(mutator->data, nullptr);
  }

  void TearDown() override {
    // Deinitialize the custom mutator
    afl_custom_deinit(mutator->data);
    mutator->data = nullptr;
    free(mutator);
    mutator = nullptr;
  }
};

TEST_F(CustomMutatorTest, FuzzNTimes) {
  uint8_t *buf = nullptr;
  size_t   buf_size = 0;

  uint8_t ret = afl_custom_queue_get(mutator->data, (const uint8_t *)"seed");
  EXPECT_EQ(ret, 1);
  afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, 4096);
  EXPECT_NE(buf, nullptr);
  afl_custom_queue_new_entry(mutator->data, (const uint8_t *)"init",
                             (const uint8_t *)"seed");

  ret = afl_custom_queue_get(mutator->data, (const uint8_t *)"init");
  EXPECT_EQ(ret, 1);
  for (int i = 0; i < num; ++i) {
    buf_size =
        afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, 4096);
    EXPECT_NE(buf, nullptr);
    string fn_new = to_string(i);
    afl_custom_queue_new_entry(mutator->data, (const uint8_t *)fn_new.c_str(),
                               (const uint8_t *)"init");

    fprintf(stderr, "=====%d=====\n", i + 1);
    dump_test_case(buf, buf_size);
  }
}

TEST_F(CustomMutatorTest, Trimming) {
  uint8_t *buf = nullptr;
  size_t   buf_size = 0;
  uint8_t  ret = 0;
  int32_t  stage_cur = 0;
  int32_t  stage_max = 0;
  size_t   old_buf_len = 0;

  // prepare a tree that has 4 recursion edges
  auto tree = tree_create();
  auto node1 = node_create(1);
  auto node2 = node_create(1);
  auto node3 = node_create_with_val(0, "{", 1);
  auto node4 = node_create(1);
  auto node5 = node_create_with_val(0, "test", 4);
  auto node6 = node_create_with_val(0, "}", 1);
  auto node7 = node_create(1);
  auto node8 = node_create_with_val(0, " ", 1);
  auto node9 = node_create(1);
  auto node10 = node_create_with_val(0, " ", 1);

  node_init_subnodes(node1, 2);
  node_set_subnode(node1, 0, node2);  // node1 -> node2
  node_set_subnode(node1, 1, node7);  // node1 -> node7

  node_init_subnodes(node2, 3);
  node_set_subnode(node2, 0, node3);
  node_set_subnode(node2, 1, node4);  // node2 -> node4
  node_set_subnode(node2, 2, node6);

  node_init_subnodes(node4, 1);
  node_set_subnode(node4, 0, node5);

  node_init_subnodes(node7, 2);
  node_set_subnode(node7, 0, node8);
  node_set_subnode(node7, 1, node9);  // node7 -> node9

  node_init_subnodes(node9, 1);
  node_set_subnode(node9, 0, node10);

  tree->root = node1;
  tree_to_buf(tree);
  tree_get_size(tree);

  trees["init"] = tree;

  ret = afl_custom_queue_get(mutator->data, (const uint8_t *)"init");
  ASSERT_EQ(ret, 1);

  // always fail in trimming
  stage_cur = 0;
  stage_max =
      afl_custom_init_trim(mutator->data, tree->data_buf, tree->data_len);
  old_buf_len = tree->data_len;
  while (stage_cur < stage_max) {
    buf_size = afl_custom_trim(mutator->data, &buf);
    EXPECT_LE(buf_size, old_buf_len);
    dump_test_case(buf, buf_size);

    stage_cur = afl_custom_post_trim(mutator->data, 0);
  }

  // always success in trimming
  stage_cur = 0;
  stage_max =
      afl_custom_init_trim(mutator->data, tree->data_buf, tree->data_len);
  old_buf_len = tree->data_len;
  while (stage_cur < stage_max) {
    buf_size = afl_custom_trim(mutator->data, &buf);
    EXPECT_LE(buf_size, old_buf_len);
    dump_test_case(buf, buf_size);

    old_buf_len = buf_size;
    stage_cur = afl_custom_post_trim(mutator->data, 1);
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc < 2) exit(EXIT_FAILURE);
  num = atoi(argv[1]);
  return RUN_ALL_TESTS();
}
