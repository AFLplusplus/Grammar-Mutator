#include "tree.h"
#include "json_c_fuzz.h"

#include "gtest/gtest.h"
#include "gtest_ext.h"

class TreeTest : public ::testing::Test {
 protected:
  tree_t *tree;
  node_t *start;
  node_t *json;
  node_t *element;
  node_t *ws_1;
  node_t *value;
  node_t *ws_2;
  node_t *sp1_1;
  node_t *ws_3;

  TreeTest() {
    tree = nullptr;
    start = nullptr;
    json = nullptr;
    element = nullptr;
    ws_1 = nullptr;
    value = nullptr;
    ws_2 = nullptr;
    sp1_1 = nullptr;
    ws_3 = nullptr;
  }

  ~TreeTest() override = default;

  void SetUp() override {
    tree = tree_create();
    start = node_create(START);
    tree->root = start;

    // start -> json
    node_init_subnodes(start, 1);
    json = node_create(JSON);
    node_set_subnode(start, 0, json);

    // json -> element
    node_init_subnodes(json, 1);
    element = node_create(ELEMENT);
    node_set_subnode(json, 0, element);

    // element -> ws_1, value ("true"), ws_2 (NULL)
    node_init_subnodes(element, 3);
    ws_1 = node_create(WS);
    value = node_create_with_val(VALUE, "true", 4);
    ws_2 = node_create(WS);
    node_set_subnode(element, 0, ws_1);
    node_set_subnode(element, 1, value);
    node_set_subnode(element, 2, ws_2);

    // ws_1 -> sp1_1 (" "), ws_3 (NULL)  (recursive)
    node_init_subnodes(ws_1, 2);
    sp1_1 = node_create_with_val(SP1, " ", 1);
    ws_3 = node_create(WS);
    node_set_subnode(ws_1, 0, sp1_1);
    node_set_subnode(ws_1, 1, ws_3);
  }

  void TearDown() override {
    tree_free(tree);
    tree = nullptr;
    start = nullptr;
    json = nullptr;
    element = nullptr;
    ws_1 = nullptr;
    value = nullptr;
    ws_2 = nullptr;
    sp1_1 = nullptr;
    ws_3 = nullptr;
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

  EXPECT_MEMEQ(" true", tree->data_buf, tree->data_len);
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
  EXPECT_EQ(tree_size, 8);

  EXPECT_EQ(start->non_term_size, 1 + json->non_term_size);
  EXPECT_EQ(start->recursion_edge_size, 1);

  EXPECT_EQ(json->non_term_size, 1 + element->non_term_size);
  EXPECT_EQ(json->recursion_edge_size, 1);

  EXPECT_EQ(
      element->non_term_size,
      1 + ws_1->non_term_size + value->non_term_size + ws_2->non_term_size);
  EXPECT_EQ(element->recursion_edge_size, 1);

  EXPECT_EQ(ws_1->non_term_size,
            1 + sp1_1->non_term_size + ws_3->non_term_size);
  EXPECT_EQ(ws_1->recursion_edge_size, 1);

  EXPECT_EQ(value->non_term_size, 1);
  EXPECT_EQ(value->recursion_edge_size, 0);

  EXPECT_EQ(ws_2->non_term_size, 1);
  EXPECT_EQ(ws_2->recursion_edge_size, 0);

  EXPECT_EQ(sp1_1->non_term_size, 1);
  EXPECT_EQ(sp1_1->recursion_edge_size, 0);

  EXPECT_EQ(ws_3->non_term_size, 1);
  EXPECT_EQ(ws_3->recursion_edge_size, 0);
}

TEST_F(TreeTest, NullNodeEqual) {
  EXPECT_TRUE(node_equal(nullptr, nullptr));

  node_t *node = node_create(0);
  EXPECT_FALSE(node_equal(node, nullptr));

  node_free(node);
}

TEST_F(TreeTest, ReplaceNode) {
  tree_get_size(tree);

  node_t *new_value = node_create_with_val(VALUE, "null", 4);

  EXPECT_TRUE(node_replace_subnode(value->parent, value, new_value));

  tree_to_buf(tree);
  EXPECT_EQ(memcmp(" null", tree->data_buf, tree->data_len), 0);

  // `value` will not be freed with the tree, because `value` has been detached
  // from the tree
  node_free(value);
}

TEST_F(TreeTest, PickNonTermNodeNeverNull) {
  node_t *picked_node = nullptr;

  node_t *_start = node_create(START);
  node_get_size(_start);
  for (int i = 0; i < 100; ++i) {
    picked_node = node_pick_non_term_subnode(_start);
    EXPECT_EQ(picked_node->non_term_size, 1);
    EXPECT_NE(picked_node->id, TERM_NODE);
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
    EXPECT_NE(picked_node->id, TERM_NODE);
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
  EXPECT_EQ(recursion_edge_list->size, 1);
  list_node_t  *head = recursion_edge_list->head;
  auto edge = (edge_t *)head->data;
  EXPECT_EQ(edge->parent, ws_1);
  EXPECT_EQ(edge->subnode, ws_3);
  EXPECT_EQ(edge->subnode_offset, 1);
}

TEST_F(TreeTest, TreeGetNonTerminalNodes) {
  EXPECT_EQ(tree->non_terminal_node_list, nullptr);
  tree_get_non_terminal_nodes(tree);
  EXPECT_NE(tree->non_terminal_node_list, nullptr);

  list_t *non_terminal_node_list = tree->non_terminal_node_list;
  EXPECT_EQ(non_terminal_node_list->size, 8);
}

TEST_F(TreeTest, TreeSerializeDeserialize) {
  tree_serialize(tree);
  EXPECT_EQ(tree->ser_len, 20 * 8 + 5);

  tree_t *new_tree = tree_deserialize(tree->ser_buf, tree->ser_len);

  EXPECT_TRUE(tree_equal(tree, new_tree));

  tree_free(new_tree);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
