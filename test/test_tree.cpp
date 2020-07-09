#include "tree.h"
#include "json_c_fuzz.h"

#include "gtest/gtest.h"

TEST(TreeTest, DumpTreeToBuffer) {
  tree_t *tree = tree_create();
  node_t *start = node_create(START);
  tree->root = start;

  // start -> json
  node_init_subnodes(start, 1);
  node_t *json = node_create(JSON);
  node_set_subnode(start, 0, json);

  // json -> element
  node_init_subnodes(json, 1);
  node_t *element = node_create(ELEMENT);
  node_set_subnode(json, 0, element);

  // element -> ws_1, value ("true"), ws_2 (NULL)
  node_init_subnodes(element, 3);
  node_t *ws_1 = node_create(WS);
  node_t *value = node_create_with_val(VALUE, "true", 4);
  node_t *ws_2 = node_create(WS);
  node_set_subnode(element, 0, ws_1);
  node_set_subnode(element, 1, value);
  node_set_subnode(element, 2, ws_2);

  // ws_1 -> sp1_1 (" "), ws_3 (NULL)
  node_init_subnodes(ws_1, 2);
  node_t *sp1_1 = node_create_with_val(SP1, " ", 1);
  node_t *ws_3 = node_create(WS);
  node_set_subnode(ws_1, 0, sp1_1);
  node_set_subnode(ws_1, 1, ws_3);

  tree_to_buf(tree);

  ASSERT_EQ(memcmp(" true", tree->data_buf, tree->data_len), 0);

  tree_free(tree);
}

TEST(TreeTest, ClonedTreeShouldEqual) {
  tree_t *tree = tree_create();
  node_t *start = node_create(START);
  tree->root = start;

  // start -> json
  node_init_subnodes(start, 1);
  node_t *json = node_create(JSON);
  node_set_subnode(start, 0, json);

  // json -> element
  node_init_subnodes(json, 1);
  node_t *element = node_create(ELEMENT);
  node_set_subnode(json, 0, element);

  // element -> ws_1, value ("true"), ws_2 (NULL)
  node_init_subnodes(element, 3);
  node_t *ws_1 = node_create(WS);
  node_t *value = node_create_with_val(VALUE, "true", 4);
  node_t *ws_2 = node_create(WS);
  node_set_subnode(element, 0, ws_1);
  node_set_subnode(element, 1, value);
  node_set_subnode(element, 2, ws_2);

  // ws_1 -> sp1_1 (" "), ws_3 (NULL)
  node_init_subnodes(ws_1, 2);
  node_t *sp1_1 = node_create_with_val(SP1, " ", 1);
  node_t *ws_3 = node_create(WS);
  node_set_subnode(ws_1, 0, sp1_1);
  node_set_subnode(ws_1, 1, ws_3);

  tree_t *new_tree = tree_clone(tree);

  ASSERT_TRUE(tree_equal(tree, new_tree));

  tree_free(tree);
  tree_free(new_tree);
}

TEST(TreeTest, ClonedTreeHaveIdenticalDataBuffer) {
  tree_t *tree = tree_create();
  node_t *start = node_create(START);
  tree->root = start;

  // start -> json
  node_init_subnodes(start, 1);
  node_t *json = node_create(JSON);
  node_set_subnode(start, 0, json);

  // json -> element
  node_init_subnodes(json, 1);
  node_t *element = node_create(ELEMENT);
  node_set_subnode(json, 0, element);

  // element -> ws_1, value ("true"), ws_2 (NULL)
  node_init_subnodes(element, 3);
  node_t *ws_1 = node_create(WS);
  node_t *value = node_create_with_val(VALUE, "true", 4);
  node_t *ws_2 = node_create(WS);
  node_set_subnode(element, 0, ws_1);
  node_set_subnode(element, 1, value);
  node_set_subnode(element, 2, ws_2);

  // ws_1 -> sp1_1 (" "), ws_3 (NULL)
  node_init_subnodes(ws_1, 2);
  node_t *sp1_1 = node_create_with_val(SP1, " ", 1);
  node_t *ws_3 = node_create(WS);
  node_set_subnode(ws_1, 0, sp1_1);
  node_set_subnode(ws_1, 1, ws_3);

  tree_t *new_tree = tree_clone(tree);

  tree_to_buf(tree);
  tree_to_buf(new_tree);

  ASSERT_EQ(tree->data_len, new_tree->data_len);
  ASSERT_EQ(memcmp(tree->data_buf, new_tree->data_buf, tree->data_len), 0);

  tree_free(tree);
  tree_free(new_tree);
}

TEST(TreeTest, TreeEqualIsNodeEqual) {
  tree_t *tree = tree_create();
  node_t *start = node_create(START);
  tree->root = start;

  // start -> json
  node_init_subnodes(start, 1);
  node_t *json = node_create(JSON);
  node_set_subnode(start, 0, json);

  // json -> element
  node_init_subnodes(json, 1);
  node_t *element = node_create(ELEMENT);
  node_set_subnode(json, 0, element);

  // element -> ws_1, value ("true"), ws_2 (NULL)
  node_init_subnodes(element, 3);
  node_t *ws_1 = node_create(WS);
  node_t *value = node_create_with_val(VALUE, "true", 4);
  node_t *ws_2 = node_create(WS);
  node_set_subnode(element, 0, ws_1);
  node_set_subnode(element, 1, value);
  node_set_subnode(element, 2, ws_2);

  // ws_1 -> sp1_1 (" "), ws_3 (NULL)
  node_init_subnodes(ws_1, 2);
  node_t *sp1_1 = node_create_with_val(SP1, " ", 1);
  node_t *ws_3 = node_create(WS);
  node_set_subnode(ws_1, 0, sp1_1);
  node_set_subnode(ws_1, 1, ws_3);

  tree_t *new_tree = tree_clone(tree);

  ASSERT_EQ(tree_equal(nullptr, nullptr), node_equal(nullptr, nullptr));
  ASSERT_EQ(tree_equal(tree, new_tree), node_equal(tree->root, new_tree->root));

  tree_free(tree);
  tree_free(new_tree);
}

TEST(TreeTest, TreeGetSize) {
  tree_t *tree = tree_create();
  node_t *start = node_create(START);
  tree->root = start;

  // start -> json
  node_init_subnodes(start, 1);
  node_t *json = node_create(JSON);
  node_set_subnode(start, 0, json);

  // json -> element
  node_init_subnodes(json, 1);
  node_t *element = node_create(ELEMENT);
  node_set_subnode(json, 0, element);

  // element -> ws_1, value ("true"), ws_2 (NULL)
  node_init_subnodes(element, 3);
  node_t *ws_1 = node_create(WS);
  node_t *value = node_create_with_val(VALUE, "true", 4);
  node_t *ws_2 = node_create(WS);
  node_set_subnode(element, 0, ws_1);
  node_set_subnode(element, 1, value);
  node_set_subnode(element, 2, ws_2);

  // ws_1 -> sp1_1 (" "), ws_3 (NULL)
  node_init_subnodes(ws_1, 2);
  node_t *sp1_1 = node_create_with_val(SP1, " ", 1);
  node_t *ws_3 = node_create(WS);
  node_set_subnode(ws_1, 0, sp1_1);
  node_set_subnode(ws_1, 1, ws_3);

  size_t tree_size = tree_get_size(tree);
  ASSERT_EQ(tree_size, 8);

  ASSERT_EQ(start->non_term_size, 1 + json->non_term_size);
  ASSERT_EQ(start->recursive_link_size, 0);

  ASSERT_EQ(json->non_term_size, 1 + element->non_term_size);
  ASSERT_EQ(json->recursive_link_size, 0);

  ASSERT_EQ(
      element->non_term_size,
      1 + ws_1->non_term_size + value->non_term_size + ws_2->non_term_size);
  ASSERT_EQ(element->recursive_link_size, 0);

  ASSERT_EQ(ws_1->non_term_size,
            1 + sp1_1->non_term_size + ws_3->non_term_size);
  ASSERT_EQ(ws_1->recursive_link_size, 1);

  ASSERT_EQ(value->non_term_size, 1);
  ASSERT_EQ(value->recursive_link_size, 0);

  ASSERT_EQ(ws_2->non_term_size, 1);
  ASSERT_EQ(ws_2->recursive_link_size, 0);

  ASSERT_EQ(sp1_1->non_term_size, 1);
  ASSERT_EQ(sp1_1->recursive_link_size, 0);

  ASSERT_EQ(ws_3->non_term_size, 1);
  ASSERT_EQ(ws_3->recursive_link_size, 0);

  tree_free(tree);
}

TEST(TreeTest, NullNodeEqual) {
  ASSERT_TRUE(node_equal(nullptr, nullptr));

  node_t *node = node_create(0);
  ASSERT_FALSE(node_equal(node, nullptr));

  node_free(node);
}

TEST(TreeTest, ReplaceNode) {
  tree_t *tree = tree_create();
  node_t *start = node_create(START);
  tree->root = start;

  // start -> json
  node_init_subnodes(start, 1);
  node_t *json = node_create(JSON);
  node_set_subnode(start, 0, json);

  // json -> element
  node_init_subnodes(json, 1);
  node_t *element = node_create(ELEMENT);
  node_set_subnode(json, 0, element);

  // element -> ws_1, value ("true"), ws_2 (NULL)
  node_init_subnodes(element, 3);
  node_t *ws_1 = node_create(WS);
  node_t *value = node_create_with_val(VALUE, "true", 4);
  node_t *ws_2 = node_create(WS);
  node_set_subnode(element, 0, ws_1);
  node_set_subnode(element, 1, value);
  node_set_subnode(element, 2, ws_2);

  // ws_1 -> sp1_1 (" "), ws_3 (NULL)
  node_init_subnodes(ws_1, 2);
  node_t *sp1_1 = node_create_with_val(SP1, " ", 1);
  node_t *ws_3 = node_create(WS);
  node_set_subnode(ws_1, 0, sp1_1);
  node_set_subnode(ws_1, 1, ws_3);

  tree_get_size(tree);

  node_t *new_value = node_create_with_val(VALUE, "null", 4);

  ASSERT_TRUE(node_replace_subnode(value->parent, value, new_value));

  tree_to_buf(tree);
  ASSERT_EQ(memcmp(" null", tree->data_buf, tree->data_len), 0);

  tree_free(tree);
  node_free(value);
}

TEST(TreeTest, PickNonTermNodeNeverNull) {
  node_t *picked_node = nullptr;

  node_t *start = node_create(START);
  node_get_size(start);
  for (int i = 0; i < 100; ++i) {
    picked_node = node_pick_non_term_subnode(start);
    ASSERT_EQ(picked_node->non_term_size, 1);
    ASSERT_NE(picked_node->id, TERM_NODE);
    ASSERT_EQ(picked_node, start);
  }

  // start -> json
  start->subnode_count = 1;
  start->subnodes = (node_t **)malloc(1 * sizeof(node_t *));
  node_t *json = node_create(1);
  node_set_subnode(start, 0, json);
  node_get_size(start);
  ASSERT_EQ(start->non_term_size, 2);

  for (int i = 0; i < 100; ++i) {
    picked_node = node_pick_non_term_subnode(start);
    ASSERT_NE(picked_node->id, TERM_NODE);
    ASSERT_TRUE(picked_node == start || picked_node == json);
  }

  node_free(start);
}
