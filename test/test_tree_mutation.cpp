#include "tree.h"
#include "tree_mutation.h"
#include "json_c_fuzz.h"

#include "gtest/gtest.h"

TEST(TreeMutationTest, RandomMutation) {
  srandom(0);  // Fix the random seed

  tree_t *tree = tree_create();
  node_t *start = node_create(START);
  tree->root = start;

  // start -> json
  start->subnodes = (node_t **)malloc(1 * sizeof(node_t *));
  node_t *json = node_create(1);
  start->subnodes[0] = json;

  // json -> element
  json->subnodes = (node_t **)malloc(1 * sizeof(node_t *));
  node_t *element = node_create(2);
  json->subnodes[0] = element;

  // element -> ws_1, value, ws_2 (NULL)
  element->subnodes = (node_t **)malloc(3 * sizeof(node_t *));
  node_t *ws_1 = node_create(3);
  node_t *value = node_create(VALUE);
  node_t *ws_2 = node_create(3);
  element->subnodes[0] = ws_1;
  element->subnodes[1] = value;
  element->subnodes[2] = ws_2;

  // ws_1 -> sp1_1 (" "), ws_3 (NULL)
  ws_1->subnodes = (node_t **)malloc(2 * sizeof(node_t *));
  node_t *sp1_1 = node_create_with_val(5, " ", 1);
  node_t *ws_3 = node_create(3);
  ws_1->subnodes[0] = sp1_1;
  ws_1->subnodes[1] = ws_3;

  // value -> object
  value->subnodes = (node_t **)malloc(1 * sizeof(node_t *));
  node_t *object = node_create(OBJECT);
  value->subnodes[0] = object;

  // object -> "{", ws_4, "}"
  object->subnodes = (node_t **)malloc(3 * sizeof(node_t *));
  node_t *ws_4 = node_create(WS);
  object->subnodes[0] = node_create_with_val(TERM_NODE, "{", 1);
  object->subnodes[1] = ws_4;
  object->subnodes[2] = node_create_with_val(TERM_NODE, "}", 1);

  // ws_4 -> sp1_2 (" "), ws_5 (NULL)
  ws_4->subnodes = (node_t **)malloc(2 * sizeof(node_t *));
  node_t *sp1_2 = node_create_with_val(SP1, " ", 1);
  node_t *ws_5 = node_create(WS);
  ws_4->subnodes[0] = sp1_2;
  ws_4->subnodes[1] = ws_5;

  tree_to_buf(tree);
  fprintf(stderr, "Before: %.*s\n", (int)tree->data_len, tree->data_buf);

  tree_t *mutated_tree = random_mutation(tree);
  bool    ret = tree_equal(mutated_tree, tree);
  EXPECT_FALSE(ret);

  tree_to_buf(mutated_tree);
  fprintf(stderr, "After: %.*s\n", (int)mutated_tree->data_len,
          mutated_tree->data_buf);

  tree_free(tree);
  tree_free(mutated_tree);
}

TEST(TreeMutationTest, RandomRecursiveMutation) {
  srandom(0);  // Fix the random seed

  tree_t *tree = tree_create();
  node_t *start = node_create(START);
  tree->root = start;

  // start -> json
  start->subnodes = (node_t **)malloc(1 * sizeof(node_t *));
  node_t *json = node_create(1);
  start->subnodes[0] = json;

  // json -> element
  json->subnodes = (node_t **)malloc(1 * sizeof(node_t *));
  node_t *element = node_create(2);
  json->subnodes[0] = element;

  // element -> ws_1, value, ws_2 (NULL)
  element->subnodes = (node_t **)malloc(3 * sizeof(node_t *));
  node_t *ws_1 = node_create(3);
  node_t *value = node_create(VALUE);
  node_t *ws_2 = node_create(3);
  element->subnodes[0] = ws_1;
  element->subnodes[1] = value;
  element->subnodes[2] = ws_2;

  // ws_1 -> sp1_1 (" "), ws_3 (NULL)
  ws_1->subnodes = (node_t **)malloc(2 * sizeof(node_t *));
  node_t *sp1_1 = node_create_with_val(5, " ", 1);
  node_t *ws_3 = node_create(3);
  ws_1->subnodes[0] = sp1_1;
  ws_1->subnodes[1] = ws_3;

  // value -> object
  value->subnodes = (node_t **)malloc(1 * sizeof(node_t *));
  node_t *object = node_create(OBJECT);
  value->subnodes[0] = object;

  // object -> "{", ws_4, "}"
  object->subnodes = (node_t **)malloc(3 * sizeof(node_t *));
  node_t *ws_4 = node_create(WS);
  object->subnodes[0] = node_create_with_val(TERM_NODE, "{", 1);
  object->subnodes[1] = ws_4;
  object->subnodes[2] = node_create_with_val(TERM_NODE, "}", 1);

  // ws_4 -> sp1_2 (" "), ws_5 (NULL)
  ws_4->subnodes = (node_t **)malloc(2 * sizeof(node_t *));
  node_t *sp1_2 = node_create_with_val(SP1, " ", 1);
  node_t *ws_5 = node_create(WS);
  ws_4->subnodes[0] = sp1_2;
  ws_4->subnodes[1] = ws_5;

  tree_to_buf(tree);
  fprintf(stderr, "Before: %.*s\n", (int)tree->data_len, tree->data_buf);

  tree_t *mutated_tree = random_recursive_mutation(tree, 1);
  bool    ret = tree_equal(mutated_tree, tree);
  // TODO: the following line may fail, as we may return the unchanged tree
  EXPECT_FALSE(ret);

  tree_to_buf(mutated_tree);
  fprintf(stderr, "After: %.*s\n", (int)mutated_tree->data_len,
          mutated_tree->data_buf);

  tree_free(tree);
  tree_free(mutated_tree);
}
