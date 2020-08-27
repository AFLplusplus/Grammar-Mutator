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

#include "tree.h"
#include "tree_mutation.h"

#include "gtest/gtest.h"
#include "gtest_ext.h"

TEST(TreeMutationTest, RandomMutation) {
  srandom(0);  // Fix the random seed

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
  srandom(0);  // Fix the random seed

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
