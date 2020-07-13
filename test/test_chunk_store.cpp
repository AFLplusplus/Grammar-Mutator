#include <map>
#include <set>
#include <vector>

#include "chunk_store.h"

#include "gtest/gtest.h"

#include "chunk_store.h"

using namespace std;

struct chunk_compare {
  bool operator()(node_t *lhs, node_t *rhs) const {
    return !node_equal(lhs, rhs);  // TODO: need to check this
  }
};

extern map<uint32_t, vector<node_t *>> chunk_store;
extern set<node_t *, chunk_compare>    seen_chunks;

class ChunkStoreTest : public ::testing::Test {
 protected:
  ChunkStoreTest() = default;

  void SetUp() override {
  }

  void TearDown() override {
    chunk_store_clear();
  }
};

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
  EXPECT_TRUE(seen_chunks.size() == 2);
  EXPECT_TRUE(chunk_store.find(1) != chunk_store.end());
  EXPECT_TRUE(chunk_store[1].size() == 2);

  tree_free(tree);
}

TEST_F(ChunkStoreTest, GetAlternativeNode) {
  EXPECT_TRUE(true);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
