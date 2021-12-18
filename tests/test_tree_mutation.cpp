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

#include <array>
#include <set>

#include "chunk_store.h"
#include "custom_mutator.h"
#include "f1_c_fuzz.h"
#include "tree.h"
#include "tree_mutation.h"
#include "utils.h"
#include "../src/chunk_store_internal.h"

#include "gtest/gtest.h"
#include "gtest_ext.h"

TEST(TreeMutationTest, RandomMutation) {

  random_set_seed(0);  // Fix the random seed

  auto tree = tree_create();
  auto node1 = node_create(1);
  tree->root = node1;
  tree_get_size(tree);

  tree_t *mutated_tree = random_mutation(tree);
  bool    ret = tree_equal(mutated_tree, tree);
  EXPECT_FALSE(ret);

  tree_free(tree);
  tree_free(mutated_tree);

}

TEST(TreeMutationTest, RandomRecursiveMutation) {

  random_set_seed(0);  // Fix the random seed

  auto tree = tree_create();
  auto node1 = node_create(1);
  auto node2 = node_create_with_val(0, "{", 1);
  auto node3 = node_create_with_val(1, "123", 3);
  auto node4 = node_create_with_val(0, "}", 1);

  node_init_subnodes(node1, 3);
  node_set_subnode(node1, 0, node2);
  node_set_subnode(node1, 1, node3);
  node_set_subnode(node1, 2, node4);
  tree->root = node1;

  tree_get_size(tree);

  tree_to_buf(tree);
  EXPECT_MEMEQ("{123}", tree->data_buf, tree->data_len);

  tree_t *mutated_tree = random_recursive_mutation(tree, 1);
  tree_to_buf(mutated_tree);
  EXPECT_MEMEQ("{{{123}}}", mutated_tree->data_buf, mutated_tree->data_len);
  bool ret = tree_equal(mutated_tree, tree);
  EXPECT_FALSE(ret);

  tree_free(tree);
  tree_free(mutated_tree);

}

TEST(TreeMutationTest, SplicingMutation) {

  random_set_seed(0);  // Fix the random seed

  auto tree1 = tree_create();  // "{" + "123" + "}"
  auto node1 = node_create(1);  // <1> -> "{" + <2> + "}"
  auto node2 = node_create_with_val(0, "{", 1);
  auto node3 = node_create(2);  // <2> -> "123"
  auto node4 = node_create_with_val(0, "}", 1);
  auto node5 = node_create_with_val(0, "123", 3);

  auto tree2 = tree_create();  // "321"
  auto node6 = node_create(2);  // <2> -> "321"
  auto node7 = node_create_with_val(0, "321", 3);

  node_init_subnodes(node1, 3);
  node_set_subnode(node1, 0, node2);
  node_set_subnode(node1, 1, node3);
  node_set_subnode(node1, 2, node4);

  node_init_subnodes(node3, 1);
  node_set_subnode(node3, 0, node5);

  node_init_subnodes(node6, 1);
  node_set_subnode(node6, 0, node7);

  tree1->root = node1;
  tree2->root = node6;

  chunk_store_init();

  // mutate NULL
  auto tree3 = splicing_mutation(nullptr);
  EXPECT_EQ(tree3, nullptr);

  // empty chunk store - return an identical tree
  tree3 = splicing_mutation(tree2);
  EXPECT_TRUE(tree_equal(tree3, tree2));

  // add tree1 and all its subtrees to the chunk store
  chunk_store_add_tree(tree1);

  auto tree4 = splicing_mutation(tree2);
  tree_to_buf(tree4);
  EXPECT_MEMEQ(tree4->data_buf, "123", 3);

  chunk_store_clear();
  tree_free(tree1);
  tree_free(tree2);
  tree_free(tree3);
  tree_free(tree4);

}

class TreeMutationUniquenessTest : public ::testing::Test {

 protected:
  std::set<std::array<char, 16+1>> tree_hash_set;

  TreeMutationUniquenessTest() = default;

  void SetUp() override { }

  void TearDown() override {

    // clean the set
    tree_hash_set.clear();

  }

  using mutation_func = std::function<tree_t *(tree_t *)>;
  void RunTest(size_t mutation_num, double ratio, const mutation_func &func) {

    random_set_seed(0);  // Fix the random seed

    // create a tree
    auto tmp_tree = gen_init__(1000);
    tree_get_size(tmp_tree);

    tree_t *tree;
    std::array<char, 16+1> tree_root_hash = {};

    for (size_t i = 0; i < mutation_num; ++i) {

      // mutate
      tree = func(tmp_tree);

      // calculate hash
      hash_node(tree->root, tree_root_hash.data());

      // insert to the set
      tree_hash_set.insert(tree_root_hash);

      // free the tree
      tree_free(tree);

    }

    // free the tree
    tree_free(tmp_tree);

    // count the number
    EXPECT_GE(tree_hash_set.size(), ratio * mutation_num);

  }

};

TEST_F(TreeMutationUniquenessTest, RandomMutation) {

  RunTest(default_random_mutation_steps, 0, random_mutation);

}

TEST_F(TreeMutationUniquenessTest, RandomRecursiveMutation) {

  const unsigned RRM_GROWTH = 10;

  RunTest(default_random_recursive_mutation_steps, 0, [&](tree_t *tree) {

    auto n = random_below(RRM_GROWTH + 1);
    return random_recursive_mutation(tree, n);

  });

}

TEST_F(TreeMutationUniquenessTest, SplicingMutation) {

  // initialize the chunk store
  chunk_store_init();

  // add 5 randomly generated trees
  for (int i = 0; i < 5; ++i) {
    tree_t *tree = gen_init__(1000);
    chunk_store_add_tree(tree);
    tree_free(tree);
  }

  RunTest(default_splicing_mutation_steps, 0, splicing_mutation);

  // clear the chunk store
  chunk_store_clear();

}
