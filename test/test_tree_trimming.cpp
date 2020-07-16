#include "tree.h"
#include "tree_trimming.h"

#include "gtest/gtest.h"
#include "gtest_ext.h"

TEST(TreeTrimmingTest, SubtreeTrimming) {
  EXPECT_TRUE(true);
}

TEST(TreeTrimmingTest, RecursiveTrimming) {
  srandom(0);  // Fix the random seed

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
