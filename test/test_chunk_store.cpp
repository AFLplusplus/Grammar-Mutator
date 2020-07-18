#include <map>
#include <set>
#include <vector>

#include "chunk_store.h"
#include "../src/chunk_store_internal.h"

#include "gtest/gtest.h"

using namespace std;

extern map<uint32_t, vector<node_t *>> chunk_store;
extern set<buffer>                     seen_chunks;

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

  buffer node1_buffer(node1);
  auto ret1 = seen_chunks.insert(node1_buffer);
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
  //  chunk_store_add_node(node1);

  EXPECT_EQ(seen_chunks.size(), chunk_store[1].size());

  node_free(node1);
}

TEST_F(ChunkStoreTest, AddTree) {
  srandom(0);  // Fix the random seed

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
  EXPECT_EQ(chunk_store[1].size(), 2);

  tree_free(tree);
}

TEST_F(ChunkStoreTest, GetAlternativeNode) {
  EXPECT_TRUE(true);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
