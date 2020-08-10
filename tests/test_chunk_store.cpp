#include <map>
#include <set>

#include "chunk_store.h"
#include "../src/chunk_store_internal.h"

#include "gtest/gtest.h"

using namespace std;

extern map<uint32_t, list_t *> chunk_store;
extern set<buffer>             seen_chunks;

extern void chunk_store_add_node(node_t *node);

class ChunkStoreTest : public ::testing::Test {
 protected:
  ChunkStoreTest() = default;

  void SetUp() override {
  }

  void TearDown() override {
    chunk_store_clear();
  }
};

TEST_F(ChunkStoreTest, SeenChunk) {
  auto node1 = node_create_with_val(1, "test", 4);
  auto node2 = node_clone(node1);

  // check the comparator
  auto ret1 = seen_chunks.insert(buffer(node1));
  EXPECT_TRUE(ret1.second);
  auto ret2 = seen_chunks.insert(buffer(node2));
  EXPECT_FALSE(ret2.second);

  EXPECT_EQ(seen_chunks.size(), 1);

  node_free(node1);
  node_free(node2);
}

TEST_F(ChunkStoreTest, AddNode) {
  auto node1 = node_create(1);
  auto node2 = node_create_with_val(0, "\r", 1);
  node_init_subnodes(node1, 1);
  node_set_subnode(node1, 0, node2);

  chunk_store_add_node(node1);
  chunk_store_add_node(node1);

  EXPECT_EQ(seen_chunks.size(), chunk_store[1]->size);

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
  EXPECT_EQ(seen_chunks.size(), 2);
  EXPECT_NE(chunk_store.find(1), chunk_store.end());
  EXPECT_EQ(chunk_store[1]->size, 2);

  tree_free(tree);
}

TEST_F(ChunkStoreTest, GetAlternativeNode) {
  // input: nullptr, output: nullptr
  EXPECT_EQ(chunk_store_get_alternative_node(nullptr), nullptr);

  // input: a node that is not in the chunk store, output: nullptr
  auto node = node_create(1);
  EXPECT_EQ(chunk_store_get_alternative_node(node), nullptr);

  chunk_store_add_node(node);
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
