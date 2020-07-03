#include "parsing_tree.h"

#include "gtest/gtest.h"

TEST(ParsingTreeTest, DumpTreeToBuffer) {
  parsing_tree_t *tree = tree_create();
  node_t *root = node_create(0);
  tree->root = root;

  // start -> json
  node_t *json = node_create(1);
  node_append_subnode(root, json);

  // json -> element
  node_t *element = node_create(2);
  node_append_subnode(json, element);

  // element -> ws_1, value ("true"), ws_2 (NULL)
  node_t *ws_1 = node_create(3);
  node_t *value = node_create_with_val(4, "true", 4);
  node_t *ws_2 = node_create(3);
  node_append_subnode(element, ws_1);
  node_append_subnode(element, value);
  node_append_subnode(element, ws_2);

  // ws_1 -> sp1_1 (" "), ws_3 (NULL)
  node_t *sp1_1 = node_create_with_val(5, " ", 1);
  node_t *ws_3 = node_create(3);
  node_append_subnode(ws_1, sp1_1);
  node_append_subnode(ws_1, ws_3);

  tree_to_buf(tree);

  ASSERT_EQ(memcmp(" true", tree->data_buf, tree->data_len), 0);

  tree_free(tree);
}
