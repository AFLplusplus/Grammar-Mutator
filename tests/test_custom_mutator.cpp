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

#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include <string>

#include "custom_mutator.h"
#include "f1_c_fuzz.h"
#include "tree.h"
#include "tree_mutation.h"
#include "utils.h"

#include "gtest/gtest.h"

using namespace std;

struct custom_mutator {

  my_mutator_t *data;

};

#ifdef DEBUG_BUILD
static void dump_test_case(uint8_t *buf, size_t buf_size) {

  fprintf(stderr, "%.*s\n", (int)buf_size, buf);

}

#endif

class CustomMutatorTest : public ::testing::Test {

 protected:
  afl_t *                afl = nullptr;
  struct custom_mutator *mutator = nullptr;
  string                 out_dir = "afl_test_fuzz_out";
  string                 tree_out_dir = out_dir + "/trees";
  string                 queue_dir = out_dir + "/queue";

  CustomMutatorTest() {

    // Check whether the directory exists
    if (!create_directory(out_dir.c_str())) {

      perror("Cannot create the output directory (CustomMutatorTest)");
      exit(EXIT_FAILURE);

    }

    if (!create_directory(tree_out_dir.c_str())) {

      perror("Cannot create the tree output directory (CustomMutatorTest)");
      exit(EXIT_FAILURE);

    }

    if (!create_directory(queue_dir.c_str())) {

      perror("Cannot create the queue output directory (CustomMutatorTest)");
      exit(EXIT_FAILURE);

    }

    // Set max tree length
    tree_set_max_len(100);

  }

  ~CustomMutatorTest() override {

    remove_directory(out_dir.c_str());

  }

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

TEST_F(CustomMutatorTest, Fuzzing) {

  uint8_t *                      buf = nullptr;
  __attribute__((unused)) size_t buf_size;

  // prepare a tree
  auto tree = gen_init__(0);
  dump_tree_to_test_case(tree, "afl_test_fuzz_out/queue/fuzz_0");
  write_tree_to_file(tree, "afl_test_fuzz_out/trees/fuzz_0");

  uint8_t ret = afl_custom_queue_get(
      mutator->data, (const uint8_t *)"afl_test_fuzz_out/queue/fuzz_0");
  EXPECT_EQ(ret, 1);

  tree_get_size(tree);
  tree_to_buf(tree);
  tree_get_recursion_edges(tree);
  int num = afl_custom_fuzz_count(mutator->data, nullptr, 0);
  int expected_num = rules_mutation_count(tree) +
                     default_random_mutation_steps +
                     default_splicing_mutation_steps;
  if (tree->recursion_edge_list->size > 0)
    expected_num += default_random_recursive_mutation_steps;
  EXPECT_EQ(num, expected_num);
  for (int i = 0; i < num; ++i) {

    buf_size = afl_custom_fuzz(mutator->data, tree->data_buf, tree->data_len,
                               &buf, nullptr, 0, 4096);
    EXPECT_NE(buf, nullptr);
    string fn_new = "afl_test_fuzz_out/queue/fuzz_0_" + to_string(i);
    afl_custom_queue_new_entry(
        mutator->data, (const uint8_t *)fn_new.c_str(),
        (const uint8_t *)"afl_test_fuzz_out/queue/fuzz_0");

#ifdef DEBUG_BUILD
    fprintf(stderr, "=====%d=====\n", i + 1);
    dump_test_case(buf, buf_size);
#endif

  }

  tree_free(tree);

}

TEST_F(CustomMutatorTest, FuzzingParsingError) {

  uint8_t *                      buf = nullptr;
  __attribute__((unused)) size_t buf_size;

  // prepare a tree that does not follow the grammar
  auto tree = tree_create();
  auto node1 = node_create_with_rule_id(1, 0);
  auto node2 = node_create_with_val(0, "\x01\x02\x03\x04", 4);
  node_init_subnodes(node1, 1);
  node_set_subnode(node1, 0, node2);
  tree->root = node1;

  dump_tree_to_test_case(tree, "afl_test_fuzz_out/queue/fuzz_parsing_error_0");
  // does not write the tree in advance, ask ANTLR4 shim to parse the test case
  //  write_tree_to_file(tree, "afl_test_fuzz_out/trees/fuzz_0");

  uint8_t ret = afl_custom_queue_get(
      mutator->data,
      (const uint8_t *)"afl_test_fuzz_out/queue/fuzz_parsing_error_0");
  EXPECT_EQ(ret, 1);

  tree_get_size(tree);
  tree_to_buf(tree);
  tree_get_recursion_edges(tree);
  int num = afl_custom_fuzz_count(mutator->data, nullptr, 0);
  int expected_num = rules_mutation_count(tree) +
                     default_random_mutation_steps +
                     default_splicing_mutation_steps;
  if (tree->recursion_edge_list->size > 0)
    expected_num += default_random_recursive_mutation_steps;
  EXPECT_NE(num, expected_num);
  for (int i = 0; i < num; ++i) {

    buf_size = afl_custom_fuzz(mutator->data, tree->data_buf, tree->data_len,
                               &buf, nullptr, 0, 4096);
    EXPECT_NE(buf, nullptr);
    string fn_new =
        "afl_test_fuzz_out/queue/fuzz_parsing_error_0_" + to_string(i);
    afl_custom_queue_new_entry(
        mutator->data, (const uint8_t *)fn_new.c_str(),
        (const uint8_t *)"afl_test_fuzz_out/queue/fuzz_parsing_error_0");

#ifdef DEBUG_BUILD
    fprintf(stderr, "=====%d=====\n", i + 1);
    dump_test_case(buf, buf_size);
#endif

  }

  tree_free(tree);

}

TEST_F(CustomMutatorTest, FuzzingNoRulesMutation) {

  uint8_t *                      buf = nullptr;
  __attribute__((unused)) size_t buf_size;

  // prepare a tree that does not follow the grammar
  auto tree = tree_create();
  auto node1 = node_create_with_rule_id(1, 0);
  auto node2 = node_create_with_val(0, "test", 4);
  node_init_subnodes(node1, 1);
  node_set_subnode(node1, 0, node2);
  tree->root = node1;

  dump_tree_to_test_case(tree, "afl_test_fuzz_out/queue/fuzz_no_rules_mut_0");
  write_tree_to_file(tree, "afl_test_fuzz_out/trees/fuzz_no_rules_mut_0");

  uint8_t ret = afl_custom_queue_get(
      mutator->data,
      (const uint8_t *)"afl_test_fuzz_out/queue/fuzz_no_rules_mut_0");
  EXPECT_EQ(ret, 1);

  tree_get_size(tree);
  tree_to_buf(tree);
  tree_get_recursion_edges(tree);
  int num = afl_custom_fuzz_count(mutator->data, nullptr, 0);
  int expected_num = rules_mutation_count(tree) +
                     default_random_mutation_steps +
                     default_splicing_mutation_steps;
  if (tree->recursion_edge_list->size > 0)
    expected_num += default_random_recursive_mutation_steps;
  EXPECT_EQ(num, expected_num);
  for (int i = 0; i < num; ++i) {

    buf_size = afl_custom_fuzz(mutator->data, tree->data_buf, tree->data_len,
                               &buf, nullptr, 0, 4096);
    EXPECT_NE(buf, nullptr);
    string fn_new =
        "afl_test_fuzz_out/queue/fuzz_no_rules_mut_0_" + to_string(i);
    afl_custom_queue_new_entry(
        mutator->data, (const uint8_t *)fn_new.c_str(),
        (const uint8_t *)"afl_test_fuzz_out/queue/fuzz_no_rules_mut_0");

#ifdef DEBUG_BUILD
    fprintf(stderr, "=====%d=====\n", i + 1);
    dump_test_case(buf, buf_size);
#endif

  }

  tree_free(tree);

}

TEST_F(CustomMutatorTest, Trimming) {

  random_set_seed(1234);

  uint8_t *                      buf = nullptr;
  __attribute__((unused)) size_t buf_size;
  uint8_t                        ret;
  int32_t                        stage_cur;
  int32_t                        stage_max;

  // prepare a tree
  auto tree = gen_init__(20);
  dump_tree_to_test_case(tree, "afl_test_fuzz_out/queue/trimming_0");
  write_tree_to_file(tree, "afl_test_fuzz_out/trees/trimming_0");

  ret = afl_custom_queue_get(
      mutator->data, (const uint8_t *)"afl_test_fuzz_out/queue/trimming_0");
  ASSERT_EQ(ret, 1);

  // always fail in trimming
  stage_cur = 0;
  stage_max =
      afl_custom_init_trim(mutator->data, tree->data_buf, tree->data_len);
  //  EXPECT_EQ(stage_max, tree->root->non_term_size +
  //  tree->root->recursion_edge_size);
  while (stage_cur < stage_max) {

    buf_size = afl_custom_trim(mutator->data, &buf);
#ifdef DEBUG_BUILD
    dump_test_case(buf, buf_size);
#endif

    stage_cur = afl_custom_post_trim(mutator->data, 0);

  }

  // always success in trimming
  stage_cur = 0;
  stage_max =
      afl_custom_init_trim(mutator->data, tree->data_buf, tree->data_len);
  //  EXPECT_EQ(stage_max, tree->root->non_term_size +
  //  tree->root->recursion_edge_size);
  while (stage_cur < stage_max) {

    buf_size = afl_custom_trim(mutator->data, &buf);
#ifdef DEBUG_BUILD
    dump_test_case(buf, buf_size);
#endif

    stage_cur = afl_custom_post_trim(mutator->data, 1);

  }

  tree_free(tree);

}

int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();

}
