#ifndef __NODE_STACK_H__
#define __NODE_STACK_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "tree.h"

// A growable stack of node frames. It replaces the call stack in the tree
// walks, so that the depth of a tree does not limit them.
typedef struct {

  node_t  *node;
  uint32_t idx;  // the child index of `node` in its parent
  size_t   aux;  // free for the caller

} node_frame_t;

typedef struct {

  node_frame_t *items;
  size_t        len;
  size_t        cap;

} node_stack_t;

// Push one frame. Return false, and leave the stack as it was, if the stack
// cannot grow. The caller must stop its walk and report the failure.
static inline bool node_stack_push(node_stack_t *s, node_t *node, uint32_t idx,
                                   size_t aux) {

  if (s->len == s->cap) {

    size_t        cap = s->cap ? s->cap * 2 : 64;
    node_frame_t *items = realloc(s->items, cap * sizeof(node_frame_t));
    if (!items) {

      perror("node_stack_push (realloc)");
      return false;

    }

    s->items = items;
    s->cap = cap;

  }

  s->items[s->len].node = node;
  s->items[s->len].idx = idx;
  s->items[s->len].aux = aux;
  ++s->len;
  return true;

}

static inline node_frame_t node_stack_pop(node_stack_t *s) {

  return s->items[--s->len];

}

// Push the non-NULL children of `node` so that the first child pops first.
// Return false if a push fails. Some children are then on the stack.
static inline bool node_stack_push_children(node_stack_t *s, node_t *node,
                                            size_t aux) {

  for (uint32_t i = node->subnode_count; i-- > 0;) {

    if (node->subnodes[i] && !node_stack_push(s, node->subnodes[i], i, aux))
      return false;

  }

  return true;

}

static inline void node_stack_free(node_stack_t *s) {

  free(s->items);
  s->items = NULL;
  s->len = s->cap = 0;

}

#endif
