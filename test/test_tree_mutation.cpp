#include "tree.h"
#include "tree_mutation.h"
#include "json_c_fuzz.h"

#include "gtest/gtest.h"

TEST(TreeMutationTest, RandomMutation) {
  tree_t *tree = tree_create();
  node_t *start = node_create(START);
  tree->root = start;

  // start -> json
  node_t *json = node_create(JSON);
  node_append_subnode(start, json);

  // json -> element
  node_t *element = node_create(ELEMENT);
  node_append_subnode(json, element);

  // element -> ws_1, value ("true"), ws_2 (NULL)
  node_t *ws_1 = node_create(WS);
  node_t *value = node_create_with_val(VALUE, "true", 4);
  node_t *ws_2 = node_create(WS);
  node_append_subnode(element, ws_1);
  node_append_subnode(element, value);
  node_append_subnode(element, ws_2);

  // ws_1 -> sp1_1 (" "), ws_3 (NULL)
  node_t *sp1_1 = node_create_with_val(SP1, " ", 1);
  node_t *ws_3 = node_create(WS);
  node_append_subnode(ws_1, sp1_1);
  node_append_subnode(ws_1, ws_3);

  tree_t *mutated_tree = random_mutation(tree);
  bool    ret = tree_equal(mutated_tree, tree);
  EXPECT_FALSE(ret);

  tree_to_buf(tree);
  fprintf(stderr, "Before: %.*s\n", (int)tree->data_len, tree->data_buf);
  tree_to_buf(mutated_tree);
  fprintf(stderr, "After: %.*s\n", (int)mutated_tree->data_len,
          mutated_tree->data_buf);

  tree_free(tree);
  tree_free(mutated_tree);
}
