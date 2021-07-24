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
#include "tree_trimming.h"
#include "utils.h"

#include "gtest/gtest.h"
#include "gtest_ext.h"

TEST(TreeTrimmingTest, SubtreeTrimming) {

  random_set_seed(0);  // Fix the random seed

  auto tree = tree_create();
  auto node1 = node_create(1);
  tree->root = node1;

  // trim `value` subnode
  tree_t *trimmed_tree = subtree_trimming(tree, node1);
  // TODO: actually, the result should be nullptr
  bool ret = tree_equal(trimmed_tree, tree);
  EXPECT_FALSE(ret);

  tree_free(tree);
  tree_free(trimmed_tree);

}

TEST(TreeTrimmingTest, RecursiveTrimming) {

  random_set_seed(0);  // Fix the random seed

  auto tree = tree_create();
  auto node1 = node_create(1);
  auto node2 = node_create_with_val(0, "{", 1);
  auto node3 = node_create(1);
  auto node4 = node_create_with_val(0, "}", 1);
  auto node5 = node_create_with_val(0, "123", 3);

  node_init_subnodes(node1, 3);
  node_set_subnode(node1, 0, node2);
  node_set_subnode(node1, 2, node4);

  node_init_subnodes(node3, 1);
  node_set_subnode(node3, 0, node5);

  auto _node = node_clone(node1);
  node_set_subnode(node1, 1, _node);
  node_set_subnode(_node, 1, node3);
  tree->root = node1;

  tree_get_size(tree);
  tree_to_buf(tree);
  EXPECT_MEMEQ("{{123}}", tree->data_buf, tree->data_len);

  edge_t  edge = {node1, _node, 1};
  tree_t *trimmed_tree = recursive_trimming(tree, edge);
  tree_to_buf(trimmed_tree);
  EXPECT_MEMEQ("{123}", trimmed_tree->data_buf, trimmed_tree->data_len);
  bool ret = tree_equal(trimmed_tree, tree);
  EXPECT_FALSE(ret);

  tree_free(tree);
  tree_free(trimmed_tree);

}
