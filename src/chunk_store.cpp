#include <map>
#include <set>
#include <vector>

#include "chunk_store.h"

using namespace std;

struct chunk_compare {
  bool operator()(node_t *lhs, node_t *rhs) const {
    return !node_equal(lhs, rhs);  // TODO: need to check this
  }
};

map<uint32_t, vector<node_t *>> chunk_store;
set<node_t *, chunk_compare>    seen_chunks;

void chunk_store_add_node(node_t *node) {
  if (!node) return;
  if (node->id == 0) return;

  // add current subtree
  if (seen_chunks.find(node) == seen_chunks.end()) {
    node_t *_node = node_clone(node);
    seen_chunks.insert(_node);
    chunk_store[_node->id].push_back(_node);
  }

  // process subnodes
  node_t *subnode = nullptr;
  for (int i = 0; i < node->subnode_count; ++i) {
    subnode = node->subnodes[i];
    chunk_store_add_node(subnode);
  }
}

void chunk_store_add_tree(tree_t *tree) {
  if (!tree) return;
  chunk_store_add_node(tree->root);
}

node_t *chunk_store_get_alternative_node(node_t *node) {
  if (!node) return nullptr;
  if (chunk_store.find(node->id) == chunk_store.end()) return nullptr;

  size_t n = chunk_store[node->id].size();
  int prob = random() % n;
  return chunk_store[node->id][prob];
}

void chunk_store_clear() {
  for (auto node : seen_chunks) {
    node_free(node);
  }
  seen_chunks.clear();
  chunk_store.clear();
}
