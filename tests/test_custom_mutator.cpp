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
  string                 out_dir = "/tmp/afl_test_fuzz_out";
  string                 tree_out_dir = out_dir + "/trees";
  string                 queue_dir = out_dir + "/queue";

  CustomMutatorTest() {
    // Check whether the directory exists
    struct stat info;
    if (stat(out_dir.c_str(), &info) != 0) {
      if (mkdir(out_dir.c_str(), 0700) != 0) {
        // error
        perror("Cannot create the directory (CustomMutatorTest)");
        exit(EXIT_FAILURE);
      }
    } else if (info.st_mode & S_IFDIR) {
      // directory exist
    } else {
      // Not a directory
      perror("Wrong output path (CustomMutatorTest)");
      exit(EXIT_FAILURE);
    }

    if (stat(tree_out_dir.c_str(), &info) != 0) {
      if (mkdir(tree_out_dir.c_str(), 0700) != 0) {
        // error
        perror("Cannot create the directory (CustomMutatorTest)");
        exit(EXIT_FAILURE);
      }
    } else if (info.st_mode & S_IFDIR) {
      // directory exist
    } else {
      // Not a directory
      perror("Wrong tree output path (CustomMutatorTest)");
      exit(EXIT_FAILURE);
    }

    if (stat(queue_dir.c_str(), &info) != 0) {
      if (mkdir(queue_dir.c_str(), 0700) != 0) {
        // error
        perror("Cannot create the directory (CustomMutatorTest)");
        exit(EXIT_FAILURE);
      }
    } else if (info.st_mode & S_IFDIR) {
      // directory exist
    } else {
      // Not a directory
      perror("Wrong queue output path (CustomMutatorTest)");
      exit(EXIT_FAILURE);
    }

    // Set max tree length
    tree_set_max_len(100);
  }

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

TEST_F(CustomMutatorTest, Fuzzing) {
  uint8_t *buf = nullptr;
  size_t   buf_size = 0;

  // prepare a tree
  auto tree = gen_init__(0);
  dump_tree_to_test_case(tree, "/tmp/afl_test_fuzz_out/queue/fuzz_0");
  write_tree_to_file(tree, "/tmp/afl_test_fuzz_out/trees/fuzz_0");

  uint8_t ret = afl_custom_queue_get(
      mutator->data, (const uint8_t *)"/tmp/afl_test_fuzz_out/queue/fuzz_0");
  EXPECT_EQ(ret, 1);

  tree_get_size(tree);
  tree_to_buf(tree);
  tree_get_recursion_edges(tree);
  int num = afl_custom_fuzz_count(mutator->data, nullptr, 0);
  int expected_num = rules_mutation_count(tree) + 100 + 100;
  if (tree->recursion_edge_list->size > 0) expected_num += 20;
  EXPECT_EQ(num, expected_num);
  for (int i = 0; i < num; ++i) {
    buf_size = afl_custom_fuzz(mutator->data, tree->data_buf, tree->data_len,
                               &buf, nullptr, 0, 4096);
    EXPECT_NE(buf, nullptr);
    string fn_new = "/tmp/afl_test_fuzz_out/queue/fuzz_0_" + to_string(i);
    afl_custom_queue_new_entry(
        mutator->data, (const uint8_t *)fn_new.c_str(),
        (const uint8_t *)"/tmp/afl_test_fuzz_out/queue/fuzz_0");

#ifdef DEBUG_BUILD
    fprintf(stderr, "=====%d=====\n", i + 1);
    dump_test_case(buf, buf_size);
#endif
  }

  tree_free(tree);
}

TEST_F(CustomMutatorTest, Trimming) {
  srandom(1234);

  uint8_t *buf = nullptr;
  size_t   buf_size = 0;
  uint8_t  ret = 0;
  int32_t  stage_cur = 0;
  int32_t  stage_max = 0;

  // prepare a tree
  auto tree = gen_init__(20);
  dump_tree_to_test_case(tree, "/tmp/afl_test_fuzz_out/queue/trimming_0");
  write_tree_to_file(tree, "/tmp/afl_test_fuzz_out/trees/trimming_0");

  ret = afl_custom_queue_get(
      mutator->data,
      (const uint8_t *)"/tmp/afl_test_fuzz_out/queue/trimming_0");
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
