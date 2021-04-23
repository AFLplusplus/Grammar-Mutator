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

#include "f1_c_fuzz.h"
#include "chunk_store.h"
#include "../src/chunk_store_internal.h"

#include "gtest/gtest.h"

using namespace std;

class ChunkStoreTest : public ::testing::Test {

 protected:
  ChunkStoreTest() = default;

  void SetUp() override {

    chunk_store_init();

  }

  void TearDown() override {

    chunk_store_clear();

  }

};

static size_t num_seen_chunks() {

  int num_seen = 0;
  const char * key;
  map_iter_t iter = map_iter(&seen_chunks);
  while ((key = map_next(&seen_chunks, &iter))) {

    ++num_seen;

  }

  return num_seen;

}

TEST_F(ChunkStoreTest, SeenChunk) {

  auto node1 = node_create_with_val(1, "test", 4);
  auto node2 = node_clone(node1);

  // check the comparator
  char node1_hash[16+1];
  hash_node(node1, node1_hash);
  EXPECT_EQ(map_set(&seen_chunks, node1_hash, node1), 0);
  char node2_hash[16+1];
  hash_node(node1, node2_hash);
  EXPECT_NE(map_get(&seen_chunks, node2_hash), nullptr);
  EXPECT_EQ(*map_get(&seen_chunks, node2_hash), node1);

  EXPECT_EQ(num_seen_chunks(), 1);

  node_free(node1);
  node_free(node2);

}

TEST_F(ChunkStoreTest, AddNode) {

  auto node1 = node_create(1);
  auto node2 = node_create_with_val(0, "\r", 1);
  node_init_subnodes(node1, 1);
  node_set_subnode(node1, 0, node2);

  // chunk store should de-duplicate the clones:
  chunk_store_take_node(node_clone(node1));
  chunk_store_take_node(node_clone(node1));
  chunk_store_take_node(node_clone(node1));
  chunk_store_take_node(node_clone(node2));
  EXPECT_EQ(num_seen_chunks(), 2);

  list_t **p_node_list = map_get(&chunk_store, node_type_str(node1->id));
  EXPECT_NE(p_node_list, nullptr);
  list_t *node_list = *p_node_list;

  // We expect only 1 node added to the node1->id matching list:
  EXPECT_EQ(node_list->size, 1);

  node_free(node1);

}

TEST_F(ChunkStoreTest, AddTree) {

  auto tree = tree_create();  // "{" + "123" + "}"
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

  chunk_store_add_tree(tree);
  EXPECT_EQ(num_seen_chunks(), 4);
  list_t **p_node_list = map_get(&chunk_store, node_type_str(node1->id));
  EXPECT_NE(p_node_list, nullptr);
  list_t *node_list = *p_node_list;
  EXPECT_EQ(node_list->size, 2);

  tree_free(tree);

}

TEST_F(ChunkStoreTest, GetAlternativeNode) {

  // input: nullptr, output: nullptr
  EXPECT_EQ(chunk_store_get_alternative_node(nullptr), nullptr);

  // input: a node that is not in the chunk store, output: nullptr
  auto node = node_create(1);
  EXPECT_EQ(chunk_store_get_alternative_node(node), nullptr);

  chunk_store_take_node(node_clone(node));
  // input: a node that has been saved in the chunk store, output: the same node
  auto _node = chunk_store_get_alternative_node(node);
  EXPECT_TRUE(node_equal(_node, node));

  node_free(node);
  node_free(_node);

}

int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();

}
