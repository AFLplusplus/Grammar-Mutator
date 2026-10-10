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

static void write_test_case(const string &fn, const uint8_t *buf,
                            size_t buf_size) {

  FILE *f = fopen(fn.c_str(), "wb");
  ASSERT_NE(f, nullptr);
  ASSERT_EQ(fwrite(buf, 1, buf_size, f), buf_size);
  fclose(f);

}

// AFL++ 5.03c and later can add a trimmed case to the queue during the trim
// loop. Then AFL++ calls `afl_custom_queue_new_entry` before the next
// `afl_custom_trim` call. See issue #57.
TEST_F(CustomMutatorTest, QueueNewEntryDuringTrim) {

  random_set_seed(1234);

  uint8_t *buf = nullptr;
  size_t   buf_size;
  int32_t  stage_cur;
  int32_t  stage_max;

  auto tree = gen_init__(20);
  dump_tree_to_test_case(tree, "afl_test_fuzz_out/queue/trim_new_entry_0");
  write_tree_to_file(tree, "afl_test_fuzz_out/trees/trim_new_entry_0");

  ASSERT_EQ(afl_custom_queue_get(
                mutator->data,
                (const uint8_t *)"afl_test_fuzz_out/queue/trim_new_entry_0"),
            1);
  tree_t *tree_cur = mutator->data->tree_cur;
  string  tree_fn_cur = mutator->data->tree_fn_cur;

  stage_cur = 0;
  stage_max =
      afl_custom_init_trim(mutator->data, tree->data_buf, tree->data_len);
  ASSERT_GT(stage_max, 1);

  int i = 0;
  while (stage_cur < stage_max) {

    buf_size = afl_custom_trim(mutator->data, &buf);
    ASSERT_NE(buf, nullptr);
    string trimmed((const char *)buf, buf_size);
    stage_cur = afl_custom_post_trim(mutator->data, 0);

    // Each trimmed case reaches new coverage
    string fn_new =
        "afl_test_fuzz_out/queue/trim_new_entry_0_" + to_string(i++);
    write_test_case(fn_new, (const uint8_t *)trimmed.data(), trimmed.size());
    afl_custom_queue_new_entry(
        mutator->data, (const uint8_t *)fn_new.c_str(),
        (const uint8_t *)"afl_test_fuzz_out/queue/trim_new_entry_0");

    // The trim loop still uses the current tree
    EXPECT_EQ(mutator->data->tree_cur, tree_cur);
    EXPECT_EQ(string(mutator->data->tree_fn_cur), tree_fn_cur);

  }

  tree_free(tree);

}

// A mutated tree from the previous queue entry must not become the tree of
// a case that AFL++ adds during the trim of the next queue entry.
TEST_F(CustomMutatorTest, QueueNewEntryDuringTrimAfterFuzzing) {

  random_set_seed(1234);

  uint8_t *buf = nullptr;
  size_t   buf_size;

  auto tree_a = gen_init__(20);
  dump_tree_to_test_case(tree_a, "afl_test_fuzz_out/queue/stale_a");
  write_tree_to_file(tree_a, "afl_test_fuzz_out/trees/stale_a");
  auto tree_b = gen_init__(20);
  dump_tree_to_test_case(tree_b, "afl_test_fuzz_out/queue/stale_b");
  write_tree_to_file(tree_b, "afl_test_fuzz_out/trees/stale_b");

  // Fuzz the first entry once. The mutated case is not interesting.
  ASSERT_EQ(afl_custom_queue_get(
                mutator->data, (const uint8_t *)"afl_test_fuzz_out/queue/stale_a"),
            1);
  ASSERT_GT(afl_custom_fuzz_count(mutator->data, nullptr, 0), 0u);
  afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, 4096);
  ASSERT_NE(buf, nullptr);

  // Trim the second entry. The first trimmed case reaches new coverage.
  ASSERT_EQ(afl_custom_queue_get(
                mutator->data, (const uint8_t *)"afl_test_fuzz_out/queue/stale_b"),
            1);
  ASSERT_GT(
      afl_custom_init_trim(mutator->data, tree_b->data_buf, tree_b->data_len),
      0);
  buf_size = afl_custom_trim(mutator->data, &buf);
  ASSERT_NE(buf, nullptr);
  string trimmed((const char *)buf, buf_size);
  afl_custom_post_trim(mutator->data, 0);

  write_test_case("afl_test_fuzz_out/queue/stale_new",
                  (const uint8_t *)trimmed.data(), trimmed.size());
  afl_custom_queue_new_entry(mutator->data,
                             (const uint8_t *)"afl_test_fuzz_out/queue/stale_new",
                             (const uint8_t *)"afl_test_fuzz_out/queue/stale_b");

  // The saved tree must match the queued case
  tree_t *saved = read_tree_from_file("afl_test_fuzz_out/trees/stale_new");
  ASSERT_NE(saved, nullptr);
  tree_to_buf(saved);
  EXPECT_EQ(string((const char *)saved->data_buf, saved->data_len), trimmed);
  tree_free(saved);

  tree_free(tree_a);
  tree_free(tree_b);

}

int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();

}
