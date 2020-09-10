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
#include "f1_c_fuzz.h"

#include "gtest/gtest.h"
#include "gtest_ext.h"

class TreeTest : public ::testing::Test {

 protected:
  tree_t *tree;
  node_t *node1;
  node_t *node2;
  node_t *node3;
  node_t *node4;
  node_t *node5;

  TreeTest() {

    tree = nullptr;
    node1 = nullptr;
    node2 = nullptr;
    node3 = nullptr;
    node4 = nullptr;
    node5 = nullptr;

  }

  ~TreeTest() override = default;

  void SetUp() override {

    tree = tree_create();
    node1 = node_create_with_rule_id(1, 1);
    node2 = node_create_with_val(0, "{", 1);
    node3 = node_create_with_rule_id(1, 0);
    node4 = node_create_with_val(0, "}", 1);
    node5 = node_create_with_val(0, "123", 3);

    node_init_subnodes(node1, 3);
    node_set_subnode(node1, 0, node2);
    node_set_subnode(node1, 2, node4);

    node_init_subnodes(node3, 1);
    node_set_subnode(node3, 0, node5);

    auto _node = node_clone(node1);
    node_set_subnode(node1, 1, _node);
    node_set_subnode(_node, 1, node3);
    tree->root = node1;

  }

  void TearDown() override {

    tree_free(tree);
    tree = nullptr;
    node1 = nullptr;
    node2 = nullptr;
    node3 = nullptr;
    node4 = nullptr;
    node5 = nullptr;

  }

};

TEST_F(TreeTest, NodeCreate) {

  auto node = node_create(0);  // terminal node

  EXPECT_EQ(node->id, 0);

  EXPECT_EQ(node->recursion_edge_size, 0);
  EXPECT_EQ(node->non_term_size, 0);

  EXPECT_EQ(node->val_buf, nullptr);
  EXPECT_EQ(node->val_size, 0);
  EXPECT_EQ(node->val_len, 0);

  EXPECT_EQ(node->parent, nullptr);

  EXPECT_EQ(node->subnodes, nullptr);
  EXPECT_EQ(node->subnode_count, 0);

  node_free(node);

}

TEST_F(TreeTest, NodeCreateWithVal) {

  auto node = node_create_with_val(0, "test", 4);  // terminal node

  EXPECT_MEMEQ(node->val_buf, "test", 4);

  node_free(node);

}

TEST_F(TreeTest, InitSubnodes) {

  // terminal node
  auto term_node = node_create_with_val(0, "test", 4);
  node_init_subnodes(term_node, 100);
  EXPECT_EQ(term_node->subnode_count, 0);
  EXPECT_EQ(term_node->subnodes, nullptr);
  node_free(term_node);

  // non-terminal node
  auto non_term_node = node_create(1);
  node_init_subnodes(non_term_node, 3);
  EXPECT_EQ(non_term_node->subnode_count, 3);
  EXPECT_NE(non_term_node->subnodes, nullptr);

  node_init_subnodes(non_term_node, 10);
  EXPECT_EQ(non_term_node->subnode_count, 10);
  EXPECT_NE(non_term_node->subnodes, nullptr);

  node_init_subnodes(non_term_node, 0);
  EXPECT_EQ(non_term_node->subnode_count, 0);
  EXPECT_EQ(non_term_node->subnodes, nullptr);

  node_free(non_term_node);

}

TEST_F(TreeTest, NodeFree) {

  auto node1 = node_create(1);
  auto node2 = node_create(2);
  auto node3 = node_create(3);
  auto node4 = node_create_with_val(0, "abc", 3);
  auto node5 = node_create_with_val(0, "xyz", 3);

  node_init_subnodes(node1, 2);
  node_set_subnode(node1, 0, node2);
  node_set_subnode(node1, 1, node3);
  node_init_subnodes(node2, 1);
  node_set_subnode(node2, 0, node4);
  node_init_subnodes(node3, 1);
  node_set_subnode(node3, 0, node5);

  node_free(node1);

  // No need for any assertions
  // This test case is mainly for Valgrind memory check

}

TEST_F(TreeTest, NodeSetVal) {

  node_set_val(nullptr, "test", 4);  // no error

  auto node = node_create(1);
  // TODO: uncomment the following three lines after updating polled string
  //  node_set_val(node, "test", 4);
  //  EXPECT_EQ(node->val_size, 0);
  //  EXPECT_EQ(node->val_buf, nullptr);

  node_set_val(node, "test", 0);
  EXPECT_EQ(node->val_size, 0);
  EXPECT_EQ(node->val_len, 0);
  EXPECT_EQ(node->val_buf, nullptr);

  node_set_val(node, nullptr, 100);
  EXPECT_EQ(node->val_size, 0);
  EXPECT_EQ(node->val_len, 0);
  EXPECT_EQ(node->val_buf, nullptr);

  node_set_val(node, "test", 4);
  EXPECT_GE(node->val_size, node->val_len);
  EXPECT_EQ(node->val_len, 4);
  EXPECT_NE(node->val_buf, nullptr);
  EXPECT_MEMEQ(node->val_buf, "test", 4);

  node_free(node);

}

TEST_F(TreeTest, NodeSetSubnode) {

  node_set_subnode(nullptr, 1000, nullptr);  // no error

  auto subnode = node_create_with_val(0, "test", 4);
  auto term_node = node_create_with_val(0, "test", 4);
  // actually, the following function call does not initialize the subnode array
  node_init_subnodes(term_node, 3);
  node_set_subnode(term_node, 0, subnode);
  EXPECT_EQ(term_node->subnode_count, 0);
  EXPECT_EQ(term_node->subnodes, nullptr);

  node_free(term_node);

  auto node = node_create(1);
  node_init_subnodes(node, 1);
  node_set_subnode(node, 1, subnode);
  EXPECT_EQ(node->subnodes[0], nullptr);

  node_set_subnode(node, 0, subnode);
  EXPECT_EQ(node->subnodes[0], subnode);
  EXPECT_EQ(subnode->parent, node);

  node_free(node);

}

TEST_F(TreeTest, DumpTreeToBuffer) {

  tree_to_buf(tree);

  EXPECT_MEMEQ("{{123}}", tree->data_buf, tree->data_len);

}

TEST_F(TreeTest, ParseTreeFromBuffer) {

  // A manually constructed tree, which does not follow the grammar
  tree_to_buf(tree);
  tree_t *recovered_tree = tree_from_buf(tree->data_buf, tree->data_len);
  EXPECT_FALSE(tree_equal(tree, recovered_tree));
  tree_free(recovered_tree);

  // Generate a correct tree based on the existing grammar
  tree_t *tree2 = gen_init__(0);
  tree_to_buf(tree2);
  tree_t *recovered_tree2 = tree_from_buf(tree2->data_buf, tree2->data_len);
  EXPECT_TRUE(tree_equal(tree2, recovered_tree2));
  tree_to_buf(recovered_tree2);
  EXPECT_MEMEQ(tree2->data_buf, recovered_tree2->data_buf,
               recovered_tree2->data_len);
  tree_free(tree2);
  tree_free(recovered_tree2);

}

TEST_F(TreeTest, ClonedTreeShouldEqual) {

  tree_t *new_tree = tree_clone(tree);

  EXPECT_TRUE(tree_equal(tree, new_tree));

  tree_free(new_tree);

}

TEST_F(TreeTest, ClonedTreeHaveIdenticalDataBuffer) {

  tree_t *new_tree = tree_clone(tree);

  tree_to_buf(tree);
  tree_to_buf(new_tree);

  EXPECT_EQ(tree->data_len, new_tree->data_len);
  EXPECT_MEMEQ(tree->data_buf, new_tree->data_buf, tree->data_len);

  tree_free(new_tree);

}

TEST_F(TreeTest, TreeEqualIsNodeEqual) {

  tree_t *new_tree = tree_clone(tree);

  EXPECT_EQ(tree_equal(nullptr, nullptr), node_equal(nullptr, nullptr));
  EXPECT_EQ(tree_equal(tree, new_tree), node_equal(tree->root, new_tree->root));

  tree_free(new_tree);

}

TEST_F(TreeTest, TreeGetSize) {

  size_t tree_size = tree_get_size(tree);
  EXPECT_EQ(tree_size, 3);

}

TEST_F(TreeTest, NullNodeEqual) {

  EXPECT_TRUE(node_equal(nullptr, nullptr));

  node_t *node = node_create(0);
  EXPECT_FALSE(node_equal(node, nullptr));

  node_free(node);

}

TEST_F(TreeTest, ReplaceNode) {

  tree_get_size(tree);

  node_t *_node = node_create_with_val(0, "null", 4);

  EXPECT_TRUE(node_replace_subnode(node5->parent, node5, _node));
  tree_to_buf(tree);
  EXPECT_EQ(memcmp("{{null}}", tree->data_buf, tree->data_len), 0);

  EXPECT_TRUE(node_replace_subnode(_node->parent, _node, node5));
  tree_to_buf(tree);
  EXPECT_EQ(memcmp("{{123}}", tree->data_buf, tree->data_len), 0);

  node_free(_node);

}

TEST_F(TreeTest, PickNonTermNodeNeverNull) {

  node_t *picked_node = nullptr;

  node_t *_start = node_create(1);
  node_get_size(_start);
  for (int i = 0; i < 100; ++i) {

    picked_node = node_pick_non_term_subnode(_start);
    EXPECT_EQ(picked_node->non_term_size, 1);
    EXPECT_NE(picked_node->id, 0);
    EXPECT_EQ(picked_node, _start);

  }

  // start -> json
  _start->subnode_count = 1;
  _start->subnodes = (node_t **)malloc(1 * sizeof(node_t *));
  node_t *_json = node_create(1);
  node_set_subnode(_start, 0, _json);
  node_get_size(_start);
  EXPECT_EQ(_start->non_term_size, 2);

  for (int i = 0; i < 100; ++i) {

    picked_node = node_pick_non_term_subnode(_start);
    EXPECT_NE(picked_node->id, 0);
    EXPECT_TRUE(picked_node == _start || picked_node == _json);

  }

  node_free(_start);

}

TEST_F(TreeTest, PickRecursionEdgeNeverNull) {

  auto node1 = node_create(1);
  auto node2 = node_create(1);
  auto node3 = node_create(1);
  auto node4 = node_create(1);
  auto node5 = node_create(2);
  auto node6 = node_create(1);
  auto node7 = node_create_with_val(0, "test", 4);

  edge_t picked_edge = {nullptr, nullptr, 0};

  node_init_subnodes(node1, 3);
  node_set_subnode(node1, 0, node2);
  node_set_subnode(node1, 1, node3);
  node_set_subnode(node1, 2, node7);
  node_init_subnodes(node3, 3);
  node_set_subnode(node3, 0, node4);
  node_set_subnode(node3, 1, node5);
  node_set_subnode(node3, 2, node6);

  node_get_size(node1);
  EXPECT_EQ(node1->recursion_edge_size, 4);
  for (int i = 0; i < 100; ++i) {

    picked_edge = node_pick_recursion_edge(node1);
    if (picked_edge.parent == node1) {

      if (picked_edge.subnode == node2) {

        EXPECT_EQ(picked_edge.subnode_offset, 0);

      } else if (picked_edge.subnode == node3) {

        EXPECT_EQ(picked_edge.subnode_offset, 1);

      } else {

        // should not reach here
        EXPECT_FALSE(true);
        break;

      }

    } else if (picked_edge.parent == node3) {

      if (picked_edge.subnode == node4) {

        EXPECT_EQ(picked_edge.subnode_offset, 0);

      } else if (picked_edge.subnode == node6) {

        EXPECT_EQ(picked_edge.subnode_offset, 2);

      } else {

        // should not reach here
        EXPECT_FALSE(true);
        break;

      }

    } else {

      // should not reach here
      EXPECT_FALSE(true);
      break;

    }

  }

  node_free(node1);

}

TEST_F(TreeTest, TreeGetRecursionEdges) {

  EXPECT_EQ(tree->recursion_edge_list, nullptr);
  tree_get_recursion_edges(tree);
  EXPECT_NE(tree->recursion_edge_list, nullptr);

  list_t *recursion_edge_list = tree->recursion_edge_list;
  EXPECT_EQ(recursion_edge_list->size, 2);
  list_node_t *head = recursion_edge_list->head;
  auto         edge = (edge_t *)head->data;
  EXPECT_EQ(edge->parent, node1);
  EXPECT_EQ(edge->subnode_offset, 1);

}

TEST_F(TreeTest, TreeGetNonTerminalNodes) {

  EXPECT_EQ(tree->non_terminal_node_list, nullptr);
  tree_get_non_terminal_nodes(tree);
  EXPECT_NE(tree->non_terminal_node_list, nullptr);

  list_t *non_terminal_node_list = tree->non_terminal_node_list;
  EXPECT_EQ(non_terminal_node_list->size, 3);

}

TEST_F(TreeTest, TreeSerializeDeserialize) {

  tree_serialize(tree);
  EXPECT_EQ(tree->ser_len, 16 * 8 + 7);

  tree_t *new_tree = tree_deserialize(tree->ser_buf, tree->ser_len);

  EXPECT_TRUE(tree_equal(tree, new_tree));

  tree_free(new_tree);

}

#if defined(ENABLE_PARSING_ARRAY_RB) && defined(ARRAY_RB_PATH)
TEST_F(TreeTest, ParseArrayRb) {

  tree_t *array_rb_tree = load_tree_from_test_case(ARRAY_RB_PATH);
  tree_to_buf(array_rb_tree);
  EXPECT_EQ(array_rb_tree->data_len, 4052);
  tree_free(array_rb_tree);

}

#endif

/**
 * A known issue: ANTLRInputStream does not accept string with special
 * character (https://github.com/antlr/antlr4/issues/2036)
 */
TEST_F(TreeTest, ParseSpecialCharacter) {

  char special_str[] = "\xFD";
  auto len = strlen(special_str);
  auto tree = tree_from_buf((const uint8_t *)special_str, len);
  EXPECT_EQ(tree, nullptr);

}

int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();

}
