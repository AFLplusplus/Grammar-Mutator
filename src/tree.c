#include "tree.h"

#define TREE_BUF_PREALLOC_SIZE (64)

inline node_t *node_create(uint32_t id) {
  node_t *node = calloc(1, sizeof(node_t));
  if (!node) {
    perror("node_create (calloc)");
    return NULL;
  }

  node->id = id;
  node->recursive_link_size = 0;
  if (id != 0) {  // "0" means the terminal node
    node->non_term_size = 1;
  }

  return node;
}

node_t *node_create_with_val(uint32_t id, const void *val_buf, size_t val_len) {
  node_t *node = node_create(id);

  node_set_val(node, val_buf, val_len);

  return node;
}

void node_init_subnodes(node_t *node, size_t n) {
  if (node == NULL) return;

  if (n == 0) {
    // clear subnode array
    if (node->subnodes) free(node->subnodes);
    node->subnode_count = 0;
    return;
  }

  if (node->subnodes) {
    node->subnodes = realloc(node->subnodes, n * sizeof(node_t *));
  } else {
    node->subnodes = calloc(n, sizeof(node_t *));
  }
  if (!node->subnodes) {
    perror("node_init_subnodes (realloc or calloc)");
    return;
  }
  node->subnode_count = n;
}

void node_free(node_t *node) {
  // val buf
  if (node->val_buf) {
    free(node->val_buf);
    node->val_buf = NULL;
    node->val_size = 0;
    node->val_len = 0;
  }

  // parent node
  node->parent = NULL;

  // subnodes
  if (node->subnode_count != 0) {
    node_t *subnode = NULL;
    for (int i = 0; i < node->subnode_count; ++i) {
      subnode = node->subnodes[i];
      node_free(subnode);
    }

    node->subnode_count = 0;
  }

  if (node->subnodes) free(node->subnodes);

  free(node);
}

void node_set_val(node_t *node, const void *val_buf, size_t val_len) {
  if (node == NULL) return;
  // TODO: the following line is necessary, but we need to update the polled
  //  string
  // if (node->id != 0) return;  // non-terminal node should not have a value
  if (val_len == 0) return;

  uint8_t *buf = maybe_grow(BUF_PARAMS(node, val), val_len);
  if (!buf) {
    perror("node_set_val (maybe_grow)");
    return;
  }

  node->val_len = val_len;
  memcpy(buf, val_buf, val_len);
}

void node_set_subnode(node_t *node, size_t i, node_t *subnode) {
  if (node == NULL) return;
  if (node->id == 0) return;  // terminal node should not have subnodes
  if (i >= node->subnode_count) return;

  node->subnodes[i] = subnode;
  subnode->parent = node;  // set the parent

  // Note that, this function does not update the `recursive_link_size` and
  // `non_term_size`
}

node_t *node_clone(node_t *node) {
  node_t *new_node = node_create(node->id);

  new_node->recursive_link_size = node->recursive_link_size;
  new_node->non_term_size = node->non_term_size;

  // val
  node_set_val(new_node, node->val_buf, node->val_len);
  new_node->val_len = node->val_len;

  // subnodes
  if (node->subnode_count != 0) {
    node_init_subnodes(new_node, node->subnode_count);
    node_t *subnode = NULL;
    for (int i = 0; i < node->subnode_count; ++i) {
      subnode = node->subnodes[i];
      node_set_subnode(new_node, i, node_clone(subnode));
    }
  }

  return new_node;
}

bool node_equal(node_t *node_a, node_t *node_b) {
  if (node_a == node_b) return true;
  if (!node_a || !node_b) return false;
  if (node_a->id != node_b->id) return false;
  if (node_a->val_len != node_b->val_len) return false;
  if (memcmp(node_a->val_buf, node_b->val_buf, node_a->val_len) != 0)
    return false;

  // Do not consider the parent node while comparing two nodes

  // subnodes
  if (node_a->subnode_count != node_b->subnode_count) return false;

  for (int i = 0; i < node_a->subnode_count; ++i) {
    if (!node_equal(node_a->subnodes[i], node_b->subnodes[i])) return false;
  }

  return true;
}

inline size_t node_get_size(node_t *node) {
  if (node == NULL) return 0;
  if (node->id == 0) return 0;  // terminal node

  node->non_term_size = 1;
  node->recursive_link_size = 0;

  node_t *subnode = NULL;
  for (int i = 0; i < node->subnode_count; ++i) {
    subnode = node->subnodes[i];
    if (node->id == subnode->id) {
      // recursive link
      ++node->recursive_link_size;
    }
    node->non_term_size += node_get_size(subnode);
  }

  return node->non_term_size;
}

bool node_replace_subnode(node_t *root, node_t *subnode, node_t *new_subnode) {
  if (!root || !subnode || !new_subnode) return false;
  if (subnode->id != new_subnode->id) return false;
  if (root != subnode->parent) return false;

  node_t *cur = NULL;
  for (int i = 0; i < root->subnode_count; ++i) {
    cur = root->subnodes[i];

    if (cur == subnode) {
      root->subnodes[i] = new_subnode;
      new_subnode->parent = root;

      // Detach `subnode` from the parent
      cur->parent = NULL;

      return true;
    }
  }

  return false;
}

node_t *node_pick_non_term_subnode(node_t *root) {
  size_t non_term_size = root->non_term_size;
  int    prob = random() % non_term_size;

  if (prob < 1) return root;
  prob -= 1;

  node_t *subnode = NULL;
  for (int i = 0; i < root->subnode_count; ++i) {
    subnode = root->subnodes[i];
    if (subnode->id == 0) continue;  // "0" means the terminal node

    if (prob < subnode->non_term_size)
      return node_pick_non_term_subnode(subnode);
    prob -= subnode->non_term_size;
  }

  // should not reach here
  return NULL;
}

void _node_to_buf(tree_t *tree, node_t *node) {
  if (!tree || !node) return;

  // dump `val` if this is a leaf node
  if (node->subnode_count == 0) {
    if (node->val_len == 0) return;

    size_t   data_len = tree->data_len;
    uint8_t *data_buf =
        maybe_grow(BUF_PARAMS(tree, data), data_len + node->val_len);
    if (!data_buf) {
      perror("tree output buffer allocation (maybe_grow)");
      return;
    }

    memcpy(data_buf + data_len, node->val_buf, node->val_len);
    tree->data_len += node->val_len;

    return;
  }

  // subnodes
  node_t *subnode = NULL;
  for (int i = 0; i < node->subnode_count; ++i) {
    subnode = node->subnodes[i];
    _node_to_buf(tree, subnode);
  }
}

inline tree_t *tree_create() {
  return calloc(1, sizeof(tree_t));
}

void tree_free(tree_t *tree) {
  // root node
  node_free(tree->root);
  tree->root = NULL;

  tree->depth = 0;

  // data buf
  if (tree->data_buf) {
    free(tree->data_buf);
    tree->data_buf = NULL;
    tree->data_size = 0;
    tree->data_len = 0;
  }

  free(tree);
}

void tree_to_buf(tree_t *tree) {
  if (!tree) return;

  maybe_grow(BUF_PARAMS(tree, data), TREE_BUF_PREALLOC_SIZE);
  tree->data_len = 0;

  _node_to_buf(tree, tree->root);
}

tree_t *tree_from_buf(const uint8_t *data_buf, size_t data_size) {
  return NULL;  // TODO: implement this function
}

tree_t *tree_clone(tree_t *tree) {
  tree_t *new_tree = tree_create();
  new_tree->root = node_clone(tree->root);
  new_tree->depth = tree->depth;

  // Do not clone the data buffer, as the cloned tree is likely for mutations
  new_tree->data_buf = NULL;
  new_tree->data_size = 0;
  new_tree->data_len = 0;

  return new_tree;
}

inline bool tree_equal(tree_t *tree_a, tree_t *tree_b) {
  if (tree_a == tree_b) return true;
  if (!tree_a || !tree_b) return false;
  return node_equal(tree_a->root, tree_b->root);
}

inline size_t tree_get_size(tree_t *tree) {
  return node_get_size(tree->root);
}
