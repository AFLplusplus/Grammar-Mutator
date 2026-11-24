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
  afl_t                 *afl = nullptr;
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

// Write a test case file, as AFL++ does before it calls
// `afl_custom_queue_new_entry`
static void write_test_case(const string &fn, const uint8_t *buf,
                            size_t buf_size) {

  FILE *f = fopen(fn.c_str(), "wb");
  ASSERT_NE(f, nullptr);
  ASSERT_EQ(fwrite(buf, 1, buf_size, f), buf_size);
  fclose(f);

}

TEST_F(CustomMutatorTest, Fuzzing) {

  uint8_t *buf = nullptr;
  size_t   buf_size;

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
    write_test_case(fn_new, buf, buf_size);
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

// Give each non-terminal node the unknown rule id, as grammar_parser does for
// token nodes. Return the number of rules mutation pairs of the tree.
static size_t set_unknown_rule_ids(node_t *node) {

  if (node->id == 0) return 0;
  node->rule_id = RULE_ID_UNKNOWN;
  size_t pairs = node_num_rules[node->id];
  for (uint32_t i = 0; i < node->subnode_count; ++i)
    pairs += set_unknown_rule_ids(node->subnodes[i]);
  return pairs;

}

TEST_F(CustomMutatorTest, FuzzingUnknownRuleIds) {

  uint8_t *buf = nullptr;

  auto   tree = gen_init__(0);
  size_t pairs = set_unknown_rule_ids(tree->root);
  ASSERT_GT(pairs, 0u);
  EXPECT_EQ(rules_mutation_count(tree), pairs);
  dump_tree_to_test_case(tree, "afl_test_fuzz_out/queue/fuzz_unknown_0");
  write_tree_to_file(tree, "afl_test_fuzz_out/trees/fuzz_unknown_0");

  ASSERT_EQ(afl_custom_queue_get(
                mutator->data,
                (const uint8_t *)"afl_test_fuzz_out/queue/fuzz_unknown_0"),
            1);
  ASSERT_GE(afl_custom_fuzz_count(mutator->data, nullptr, 0), pairs);

  // Each rules mutation step has a node and a rule to try, also the last one.
  tree_to_buf(tree);
  for (size_t i = 0; i < pairs; ++i) {

    buf = nullptr;
    size_t buf_size = afl_custom_fuzz(mutator->data, tree->data_buf,
                                      tree->data_len, &buf, nullptr, 0, 4096);
    EXPECT_NE(buf, nullptr) << i;
    EXPECT_GT(buf_size, 0u) << i;

  }

  tree_free(tree);

}

TEST_F(CustomMutatorTest, FuzzingParsingError) {

  uint8_t *buf = nullptr;
  size_t   buf_size;

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
    write_test_case(fn_new, buf, buf_size);
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

  uint8_t *buf = nullptr;
  size_t   buf_size;

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
    write_test_case(fn_new, buf, buf_size);
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

  uint8_t *buf = nullptr;
  size_t   buf_size;
  uint8_t  ret;
  int32_t  stage_cur;
  int32_t  stage_max;

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

// The sizes of a node can be larger than its node and edge lists. For example,
// a list walk that runs out of memory gives a short list. The trim stages take
// their step counts from the lists, so each step has a node or an edge.
TEST_F(CustomMutatorTest, TrimmingWithSizesLargerThanTheLists) {

  for (int success = 0; success <= 1; ++success) {

    random_set_seed(1234);
    auto         tree = gen_init__(20);
    const string fn = queue_dir + "/trim_sizes_" + to_string(success);
    dump_tree_to_test_case(tree, fn.c_str());
    ASSERT_EQ(afl_custom_queue_get(mutator->data, (const uint8_t *)fn.c_str()),
              1);

    tree_t *cur = mutator->data->tree_cur;
    ASSERT_NE(cur, nullptr);
    cur->root->non_term_size += 5;
    cur->root->recursion_edge_size += 5;

    int32_t stage_max = afl_custom_init_trim(mutator->data, nullptr, 0);
    EXPECT_EQ((size_t)stage_max, cur->non_terminal_node_list->size +
                                     cur->recursion_edge_list->size);

    int32_t stage_cur = 0;
    while (stage_cur < stage_max) {

      uint8_t *buf = nullptr;
      afl_custom_trim(mutator->data, &buf);
      stage_cur = afl_custom_post_trim(mutator->data, success);

    }

    tree_free(tree);

  }

}

// A failed size walk sets the root sizes to 0. The fuzz stages compute the
// sizes again, so the random and splicing stages find a node.
TEST_F(CustomMutatorTest, FuzzingWithStaleRootSizes) {

  random_set_seed(1234);
  auto         tree = gen_init__(20);
  const string fn = queue_dir + "/fuzz_stale_sizes";
  dump_tree_to_test_case(tree, fn.c_str());
  ASSERT_EQ(afl_custom_queue_get(mutator->data, (const uint8_t *)fn.c_str()),
            1);
  ASSERT_NE(mutator->data->tree_cur, nullptr);
  mutator->data->tree_cur->root->non_term_size = 0;
  mutator->data->tree_cur->root->recursion_edge_size = 0;

  uint32_t num = afl_custom_fuzz_count(mutator->data, nullptr, 0);
  ASSERT_GT(num, 0u);
  for (uint32_t i = 0; i < num; ++i) {

    uint8_t *buf = nullptr;
    afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, 4096);
    ASSERT_NE(buf, nullptr) << i;

  }

  tree_free(tree);

}

// A tree with a terminal root has no node to mutate or trim
TEST_F(CustomMutatorTest, TerminalRootHasNoFuzzOrTrimSteps) {

  auto tree = tree_create();
  tree->root = node_create_with_val(0, "x", 1);
  const string fn = queue_dir + "/terminal_root";
  const string tree_fn = tree_out_dir + "/terminal_root";
  dump_tree_to_test_case(tree, fn.c_str());
  write_tree_to_file(tree, tree_fn.c_str());
  ASSERT_EQ(afl_custom_queue_get(mutator->data, (const uint8_t *)fn.c_str()),
            1);
  ASSERT_NE(mutator->data->tree_cur, nullptr);
  ASSERT_EQ(mutator->data->tree_cur->root->id, 0u);

  EXPECT_EQ(afl_custom_fuzz_count(mutator->data, nullptr, 0), 0u);
  EXPECT_EQ(afl_custom_init_trim(mutator->data, nullptr, 0), 0);

  tree_free(tree);

}

// The labels that `afl_custom_describe` gives for the stages 0 to 3
static const char *const kStageLabels[] = {"op:gm-rules", "op:gm-random",
                                           "op:gm-recursive", "op:gm-splice"};

// Write a random tree as a queue entry and its tree file. Return the name of
// the queue entry.
static string queue_random_tree(const string &queue_dir,
                                const string &tree_out_dir,
                                const string &name) {

  auto         tree = gen_init__(0);
  const string fn = queue_dir + "/" + name;
  dump_tree_to_test_case(tree, fn.c_str());
  write_tree_to_file(tree, (tree_out_dir + "/" + name).c_str());
  tree_free(tree);
  return fn;

}

// AFL++ calls `afl_custom_describe` after `afl_custom_fuzz` returns. At the
// last step of a stage, `afl_custom_fuzz` already moved to the next stage. The
// label must name the stage that made the output.
TEST_F(CustomMutatorTest, DescribeNamesTheStageOfEachOutput) {

  const string fn = queue_random_tree(queue_dir, tree_out_dir, "describe");
  ASSERT_EQ(afl_custom_queue_get(mutator->data, (const uint8_t *)fn.c_str()),
            1);
  uint32_t num = afl_custom_fuzz_count(mutator->data, nullptr, 0);
  ASSERT_GT(num, 0u);

  size_t last_steps = 0;
  for (uint32_t i = 0; i < num; ++i) {

    uint8_t stage = mutator->data->cur_fuzzing_stage;
    ASSERT_LT(stage, 4u);
    uint8_t *buf = nullptr;
    afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, 4096);
    ASSERT_NE(buf, nullptr);

    const char *label = afl_custom_describe(mutator->data, 256);
    ASSERT_NE(label, nullptr) << "step " << i;
    EXPECT_STREQ(label, kStageLabels[stage]) << "step " << i;
    if (mutator->data->cur_fuzzing_stage != stage) ++last_steps;

  }

  // The test must include the last step of at least one stage
  EXPECT_GE(last_steps, 1u);

}

// AFL++ gives the space left in the file name. The label must fit in it.
TEST_F(CustomMutatorTest, DescribeCutsToTheMaximumLength) {

  const string fn = queue_random_tree(queue_dir, tree_out_dir, "describe_len");
  ASSERT_EQ(afl_custom_queue_get(mutator->data, (const uint8_t *)fn.c_str()),
            1);
  ASSERT_GT(afl_custom_fuzz_count(mutator->data, nullptr, 0), 0u);
  uint8_t *buf = nullptr;
  afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, 4096);
  ASSERT_NE(buf, nullptr);

  // Each label starts with "op:gm-"
  EXPECT_STREQ(afl_custom_describe(mutator->data, 6), "op:gm");
  EXPECT_EQ(afl_custom_describe(mutator->data, 0), nullptr);

}

// Without an output of the current entry, there is no label. AFL++ then uses
// its own description.
TEST_F(CustomMutatorTest, DescribeIsNullBeforeTheFirstOutput) {

  const string fn = queue_random_tree(queue_dir, tree_out_dir, "describe_new");
  ASSERT_EQ(afl_custom_queue_get(mutator->data, (const uint8_t *)fn.c_str()),
            1);
  EXPECT_EQ(afl_custom_describe(mutator->data, 256), nullptr);

  ASSERT_GT(afl_custom_fuzz_count(mutator->data, nullptr, 0), 0u);
  uint8_t *buf = nullptr;
  afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, 4096);
  ASSERT_NE(buf, nullptr);
  EXPECT_NE(afl_custom_describe(mutator->data, 256), nullptr);

  // The next entry starts with no label again
  ASSERT_EQ(afl_custom_queue_get(mutator->data, (const uint8_t *)fn.c_str()),
            1);
  EXPECT_EQ(afl_custom_describe(mutator->data, 256), nullptr);

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
  ASSERT_EQ(
      afl_custom_queue_get(mutator->data,
                           (const uint8_t *)"afl_test_fuzz_out/queue/stale_a"),
      1);
  ASSERT_GT(afl_custom_fuzz_count(mutator->data, nullptr, 0), 0u);
  afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, 4096);
  ASSERT_NE(buf, nullptr);

  // Trim the second entry. The first trimmed case reaches new coverage.
  ASSERT_EQ(
      afl_custom_queue_get(mutator->data,
                           (const uint8_t *)"afl_test_fuzz_out/queue/stale_b"),
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
  afl_custom_queue_new_entry(
      mutator->data, (const uint8_t *)"afl_test_fuzz_out/queue/stale_new",
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

static void expect_no_tree_file(const string &fn) {

  FILE *f = fopen(fn.c_str(), "rb");
  EXPECT_EQ(f, nullptr) << fn;
  if (f) fclose(f);

}

static string valid_test_case() {

  tree_t *gen = gen_init__(0);
  tree_to_buf(gen);
  string content((const char *)gen->data_buf, gen->data_len);
  tree_free(gen);
  return content;

}

TEST_F(CustomMutatorTest, RejectedEntryReturnsOne) {

  const string fn = queue_dir + "/rejected_0";
  write_test_case(fn, (const uint8_t *)"\xFD", 1);

  EXPECT_EQ(afl_custom_queue_get(mutator->data, (const uint8_t *)fn.c_str()),
            1);
  EXPECT_EQ(mutator->data->tree_cur, nullptr);
  EXPECT_EQ(afl_custom_fuzz_count(mutator->data, nullptr, 0), 0u);
  EXPECT_EQ(afl_custom_init_trim(mutator->data, nullptr, 0), 0);
  expect_no_tree_file(tree_out_dir + "/rejected_0");

}

TEST_F(CustomMutatorTest, RejectedEntryIsNotParsedAgain) {

  const string fn = queue_dir + "/rejected_1";
  write_test_case(fn, (const uint8_t *)"\xFD", 1);
  EXPECT_EQ(afl_custom_queue_get(mutator->data, (const uint8_t *)fn.c_str()),
            1);
  EXPECT_EQ(mutator->data->tree_cur, nullptr);

  // The file is valid now, but the set answers
  string valid = valid_test_case();
  write_test_case(fn, (const uint8_t *)valid.data(), valid.size());
  EXPECT_EQ(afl_custom_queue_get(mutator->data, (const uint8_t *)fn.c_str()),
            1);
  EXPECT_EQ(mutator->data->tree_cur, nullptr);

  // Another entry name is a new key
  const string fn2 = queue_dir + "/accepted_1";
  write_test_case(fn2, (const uint8_t *)valid.data(), valid.size());
  EXPECT_EQ(afl_custom_queue_get(mutator->data, (const uint8_t *)fn2.c_str()),
            1);
  EXPECT_NE(mutator->data->tree_cur, nullptr);

}

TEST_F(CustomMutatorTest, RejectedEntryOutsideQueue) {

  const string dir = out_dir + "/other";
  ASSERT_TRUE(create_directory(dir.c_str()));
  const string fn = dir + "/rejected_2";
  write_test_case(fn, (const uint8_t *)"\xFD", 1);
  EXPECT_EQ(afl_custom_queue_get(mutator->data, (const uint8_t *)fn.c_str()),
            1);
  EXPECT_EQ(mutator->data->tree_cur, nullptr);

  string valid = valid_test_case();
  write_test_case(fn, (const uint8_t *)valid.data(), valid.size());
  EXPECT_EQ(afl_custom_queue_get(mutator->data, (const uint8_t *)fn.c_str()),
            1);
  EXPECT_EQ(mutator->data->tree_cur, nullptr);

}

// Read the tree file of a queue entry and give its bytes
static string tree_file_text(const string &fn) {

  tree_t *saved = read_tree_from_file(fn.c_str());
  if (!saved) return "<no tree file>";
  tree_to_buf(saved);
  string text((const char *)saved->data_buf, saved->data_len);
  tree_free(saved);
  return text;

}

// An entry without a tree: the fuzz stages return no data, and the trim
// callbacks do nothing.
TEST_F(CustomMutatorTest, NoTreeEntryFuzzTrimAndPostTrim) {

  const string fn = queue_dir + "/notree_0";
  write_test_case(fn, (const uint8_t *)"\xFD", 1);
  ASSERT_EQ(afl_custom_queue_get(mutator->data, (const uint8_t *)fn.c_str()),
            1);
  ASSERT_EQ(mutator->data->tree_cur, nullptr);

  uint8_t *buf = (uint8_t *)0x1;
  EXPECT_EQ(afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, 4096),
            0u);
  EXPECT_EQ(buf, nullptr);

  EXPECT_EQ(afl_custom_init_trim(mutator->data, nullptr, 0), 0);
  buf = (uint8_t *)0x1;
  EXPECT_EQ(afl_custom_trim(mutator->data, &buf), 0u);
  EXPECT_EQ(buf, nullptr);
  EXPECT_EQ(afl_custom_post_trim(mutator->data, 1), 0);
  EXPECT_EQ(afl_custom_post_trim(mutator->data, 0), 0);
  expect_no_tree_file(tree_out_dir + "/notree_0");

}

// A new entry that the parser rejects gets no tree file. The callback still
// returns 0 (the queue file does not change).
TEST_F(CustomMutatorTest, QueueNewEntryOfAnInvalidCaseWritesNoTree) {

  const string orig = queue_dir + "/new_orig";
  const string fn = queue_dir + "/new_bad";
  write_test_case(orig, (const uint8_t *)"x", 1);
  write_test_case(fn, (const uint8_t *)"\xFD", 1);

  EXPECT_EQ(
      afl_custom_queue_new_entry(mutator->data, (const uint8_t *)fn.c_str(),
                                 (const uint8_t *)orig.c_str()),
      0);
  expect_no_tree_file(tree_out_dir + "/new_bad");
  EXPECT_EQ(mutator->data->mutated_tree, nullptr);

}

// An initial entry (no original file name) goes through `queue_get`, which
// writes its tree file.
TEST_F(CustomMutatorTest, QueueNewEntryWithoutOriginalParsesTheEntry) {

  const string fn = queue_dir + "/new_init";
  string       valid = valid_test_case();
  write_test_case(fn, (const uint8_t *)valid.data(), valid.size());

  EXPECT_EQ(afl_custom_queue_new_entry(mutator->data,
                                       (const uint8_t *)fn.c_str(), nullptr),
            0);
  EXPECT_NE(mutator->data->tree_cur, nullptr);
  EXPECT_EQ(tree_file_text(tree_out_dir + "/new_init"), valid);

}

// Another AFL++ stage (for example havoc or cmplog) can find a new entry after
// the custom stage. Then the last mutated tree is not the tree of the entry.
TEST_F(CustomMutatorTest, QueueNewEntryFromAnotherStage) {

  random_set_seed(1234);

  uint8_t *buf = nullptr;
  size_t   buf_size;

  auto tree = gen_init__(20);
  dump_tree_to_test_case(tree, "afl_test_fuzz_out/queue/other_stage");
  write_tree_to_file(tree, "afl_test_fuzz_out/trees/other_stage");
  ASSERT_EQ(afl_custom_queue_get(
                mutator->data,
                (const uint8_t *)"afl_test_fuzz_out/queue/other_stage"),
            1);

  // The last custom fuzz output is not interesting
  ASSERT_GT(afl_custom_fuzz_count(mutator->data, nullptr, 0), 0u);
  buf_size = afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, 4096);
  ASSERT_NE(buf, nullptr);
  string mutated((const char *)buf, buf_size);

  // Another stage finds a different valid test case
  string valid = valid_test_case();
  ASSERT_NE(valid, mutated);
  const string fn_new = queue_dir + "/other_stage_new";
  write_test_case(fn_new, (const uint8_t *)valid.data(), valid.size());
  afl_custom_queue_new_entry(
      mutator->data, (const uint8_t *)fn_new.c_str(),
      (const uint8_t *)"afl_test_fuzz_out/queue/other_stage");

  EXPECT_EQ(tree_file_text(tree_out_dir + "/other_stage_new"), valid);

  tree_free(tree);

}

// AFL++ queues the custom fuzz output, which max_size can cut. The tree file
// must be the parse result of the queued file, not the uncut mutated tree.
TEST_F(CustomMutatorTest, QueueNewEntryOfACutOutput) {

  random_set_seed(1234);

  uint8_t *buf = nullptr;
  size_t   buf_size = 0;

  auto tree = gen_init__(20);
  tree_to_buf(tree);
  ASSERT_GT(tree->data_len, 2u);
  dump_tree_to_test_case(tree, "afl_test_fuzz_out/queue/cut_output");
  write_tree_to_file(tree, "afl_test_fuzz_out/trees/cut_output");
  ASSERT_EQ(
      afl_custom_queue_get(
          mutator->data, (const uint8_t *)"afl_test_fuzz_out/queue/cut_output"),
      1);

  // Find a mutation that max_size cuts
  uint32_t count = afl_custom_fuzz_count(mutator->data, nullptr, 0);
  size_t   max_size = tree->data_len / 2;
  bool     cut = false;
  for (uint32_t i = 0; i < count && !cut; ++i) {

    buf_size =
        afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, max_size);
    ASSERT_NE(buf, nullptr);
    cut = mutator->data->mutated_tree->data_len > buf_size;

  }

  ASSERT_TRUE(cut);
  tree_t *mutated_tree = mutator->data->mutated_tree;
  string  uncut((const char *)mutated_tree->data_buf, mutated_tree->data_len);
  string  queued((const char *)buf, buf_size);

  // The parse result of the queued bytes
  string  expected = "<no tree file>";
  tree_t *parsed = tree_from_buf((const uint8_t *)queued.data(), queued.size());
  if (parsed) {

    tree_to_buf(parsed);
    expected = string((const char *)parsed->data_buf, parsed->data_len);
    tree_free(parsed);

  }

  const string fn_new = queue_dir + "/cut_output_new";
  write_test_case(fn_new, (const uint8_t *)queued.data(), queued.size());
  afl_custom_queue_new_entry(
      mutator->data, (const uint8_t *)fn_new.c_str(),
      (const uint8_t *)"afl_test_fuzz_out/queue/cut_output");

  string text = tree_file_text(tree_out_dir + "/cut_output_new");
  EXPECT_NE(text, uncut);
  EXPECT_EQ(text, expected);

  tree_free(tree);

}

// The usual case: AFL++ queues the custom fuzz output as it is
TEST_F(CustomMutatorTest, QueueNewEntryOfTheFuzzOutput) {

  random_set_seed(1234);

  uint8_t *buf = nullptr;
  size_t   buf_size;

  auto tree = gen_init__(20);
  dump_tree_to_test_case(tree, "afl_test_fuzz_out/queue/fuzz_output");
  write_tree_to_file(tree, "afl_test_fuzz_out/trees/fuzz_output");
  ASSERT_EQ(afl_custom_queue_get(
                mutator->data,
                (const uint8_t *)"afl_test_fuzz_out/queue/fuzz_output"),
            1);

  ASSERT_GT(afl_custom_fuzz_count(mutator->data, nullptr, 0), 0u);
  buf_size = afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, 4096);
  ASSERT_NE(buf, nullptr);
  string mutated((const char *)buf, buf_size);

  const string fn_new = queue_dir + "/fuzz_output_new";
  write_test_case(fn_new, (const uint8_t *)mutated.data(), mutated.size());
  afl_custom_queue_new_entry(
      mutator->data, (const uint8_t *)fn_new.c_str(),
      (const uint8_t *)"afl_test_fuzz_out/queue/fuzz_output");

  EXPECT_EQ(tree_file_text(tree_out_dir + "/fuzz_output_new"), mutated);
  EXPECT_EQ(mutator->data->mutated_tree, nullptr);

  tree_free(tree);

}

TEST(CustomMutatorEnv, ParseTimeBudgetOff) {

  const char *old = getenv("PARSE_TIME_BUDGET");
  string      saved = old ? old : "";

  setenv("PARSE_TIME_BUDGET", "0", 1);
  my_mutator_t *data = afl_custom_init(nullptr, 0);
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(data->budget, nullptr);
  afl_custom_deinit(data);

  setenv("PARSE_TIME_BUDGET", "1", 1);
  data = afl_custom_init(nullptr, 0);
  ASSERT_NE(data, nullptr);
  EXPECT_NE(data->budget, nullptr);
  afl_custom_deinit(data);

  unsetenv("PARSE_TIME_BUDGET");
  data = afl_custom_init(nullptr, 0);
  ASSERT_NE(data, nullptr);
  EXPECT_NE(data->budget, nullptr);
  afl_custom_deinit(data);

  if (old) {

    setenv("PARSE_TIME_BUDGET", saved.c_str(), 1);

  } else {

    unsetenv("PARSE_TIME_BUDGET");

  }

}

int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();

}
