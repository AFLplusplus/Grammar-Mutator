#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "helpers.h"
#include "tree.h"
#include "custom_mutator.h"
#include "json_c_fuzz.h"
#include "tree_mutation.h"
#include "tree_trimming.h"
#include "chunk_store.h"

using namespace std;

map<string, tree_t *> trees;

my_mutator_t *afl_custom_init(afl_t *afl, unsigned int seed) {
  srandom(seed);

  my_mutator_t *data = (my_mutator_t *)calloc(1, sizeof(my_mutator_t));
  if (!data) {
    perror("afl_custom_init alloc");
    return nullptr;
  }

  data->afl = afl;

  return data;
}

void afl_custom_deinit(my_mutator_t *data) {
  if (data->mutated_tree) tree_free(data->mutated_tree);
  if (data->trimmed_tree) tree_free(data->trimmed_tree);

  data->cur_trimming_stage = 0;
  data->cur_subtree_trimming_step = 0;
  data->total_subtree_trimming_steps = 0;
  data->cur_recursive_trimming_step = 0;
  data->total_recursive_trimming_steps = 0;

  // trees
  for (auto &kv : trees) {
    tree_free(kv.second);
  }
  trees.clear();

  free(data->fuzz_buf);
  free(data);

  chunk_store_clear();
}

// For each interesting test case in the queue
uint8_t afl_custom_queue_get(my_mutator_t *data, const uint8_t *filename) {
  string fn((const char *)filename);
  data->filename_cur = filename;
  data->tree_cur = nullptr;

  if (trees.find(fn) != trees.end()) data->tree_cur = trees[fn];

  if (data->tree_cur) {
    tree_get_size(data->tree_cur);
    return 1;
  }

  // TODO: Read the test case from the file and parse it

  return 1;
}

// Trimming
int32_t afl_custom_init_trim(my_mutator_t *data, uint8_t *buf,
                             size_t buf_size) {
  if (!data->tree_cur) return 0;

  tree_get_non_terminal_nodes(data->tree_cur);
  tree_get_recursion_edges(data->tree_cur);

  data->cur_trimming_stage = 0;

  data->cur_subtree_trimming_step = 0;
  data->total_subtree_trimming_steps = data->tree_cur->root->non_term_size;

  data->cur_recursive_trimming_step = 0;
  data->total_recursive_trimming_steps =
      data->tree_cur->root->recursion_edge_size;

  return data->total_subtree_trimming_steps +
         data->total_recursive_trimming_steps;
}

size_t afl_custom_trim(my_mutator_t *data, uint8_t **out_buf) {
  tree_t *trimmed_tree = nullptr;
  size_t  trimmed_size = 0;
  tree_t *tree_cur = data->tree_cur;

  if (data->cur_trimming_stage == 0) {
    // subtree trimming
    node_t *node = (node_t *)list_pop_front(tree_cur->non_terminal_node_list);
    trimmed_tree = subtree_trimming(tree_cur, node);

    ++data->cur_subtree_trimming_step;
  } else if (data->cur_trimming_stage == 1) {
    // recursive trimming
    edge_t *edge = (edge_t *)list_pop_front(tree_cur->recursion_edge_list);
    trimmed_tree = recursive_trimming(tree_cur, *edge);
    free(edge);

    ++data->cur_recursive_trimming_step;
  } else {
    // should not reach here
    *out_buf = nullptr;
    perror("wrong trimming stage");
    return 0;
  }

  tree_to_buf(trimmed_tree);
  tree_get_size(trimmed_tree);
  data->trimmed_tree = trimmed_tree;

  // maybe_grow is optimized to be quick for reused buffers.
  trimmed_size = trimmed_tree->data_len;
  uint8_t *trimmed_out =
      (uint8_t *)maybe_grow(BUF_PARAMS(data, fuzz), trimmed_size);
  if (!trimmed_out) {
    *out_buf = nullptr;
    perror("custom mutator allocation (maybe_grow)");
    return 0; /* afl-fuzz will very likely error out after this. */
  }

  memcpy(trimmed_out, trimmed_tree->data_buf, trimmed_size);
  *out_buf = trimmed_out;

  return trimmed_size;
}

int32_t afl_custom_post_trim(my_mutator_t *data, int success) {
  if (success) {
    // update the trimming step
    size_t remaining_node_size = data->tree_cur->non_terminal_node_list->size;
    size_t remaining_edge_size = data->tree_cur->recursion_edge_list->size;

    // update the corresponding tree
    string fn((const char *)data->filename_cur);
    trees[fn] = data->trimmed_tree;
    tree_free(data->tree_cur);
    data->tree_cur = data->trimmed_tree;

    // update the non-terminal node list
    tree_get_non_terminal_nodes(data->tree_cur);
    for (int i = 0; i < data->cur_subtree_trimming_step; ++i)
      list_pop_front(data->tree_cur->non_terminal_node_list);
    size_t new_node_size = data->tree_cur->non_terminal_node_list->size;

    data->cur_subtree_trimming_step += (remaining_node_size - new_node_size);

    // update the recursion edge list
    tree_get_recursion_edges(data->tree_cur);
    size_t new_edge_size = data->tree_cur->recursion_edge_list->size;

    data->cur_recursive_trimming_step +=
        (remaining_edge_size - new_edge_size);
  } else {
    // the trimmed tree will not be saved, so destroy it
    tree_free(data->trimmed_tree);
  }
  data->trimmed_tree = nullptr;

  // update trimming stage
  if (data->cur_trimming_stage == 0) {
    if (data->cur_subtree_trimming_step >= data->total_subtree_trimming_steps)
      ++data->cur_trimming_stage;
  }

  if (data->cur_trimming_stage == 1) {
    if (data->cur_recursive_trimming_step >= data->total_recursive_trimming_steps)
      ++data->cur_trimming_stage;
  }

  return data->cur_subtree_trimming_step + data->cur_recursive_trimming_step;
}

// Fuzz the given test case several times, which is defined by the
// `custom_mutator_stage` in `afl-fuzz-one.c`
size_t afl_custom_fuzz(my_mutator_t *data, uint8_t *buf, size_t buf_size,
                       uint8_t **out_buf, uint8_t *add_buf,
                       size_t add_buf_size,  // add_buf can be NULL
                       size_t max_size) {
  tree_t *tree = nullptr;
  size_t  mutated_size = 0;

  if (data->mutated_tree) {
    /* `data->mutated_tree` is NULL, meaning that this is not an interesting
      mutation (`afl_custom_queue_new_entry` is not invoked). Therefore, we
      need to free the memory. */
    tree_free(data->mutated_tree);
    data->mutated_tree = nullptr;
  }

  tree = data->tree_cur;
  if (!tree) {
    // Generation
    // Randomly generate a JSON string
    max_depth =
        random() % 15 + 1;  // randomly pick a `max_depth` within [1, 15]
    tree = gen_init__();
  } else {
    int mutation_choice = random() % 3;
    switch (mutation_choice) {
      case 0:
        // random mutation
        tree = random_mutation(tree);
        break;
      case 1:
        // random recursive mutation
        tree = random_recursive_mutation(tree, random() % 6);
        break;
      case 2:
        // splicing mutation
        tree = splicing_mutation(tree);
        break;
      default:
        break;
    }

    if (!tree) {
      perror("random mutation");
      return 0;
    }
  }

  tree_to_buf(tree);
  tree_get_size(tree);
  data->mutated_tree = tree;
  mutated_size = tree->data_len <= max_size ? tree->data_len : max_size;

  // maybe_grow is optimized to be quick for reused buffers.
  uint8_t *mutated_out =
      (uint8_t *)maybe_grow(BUF_PARAMS(data, fuzz), mutated_size);
  if (!mutated_out) {
    *out_buf = nullptr;
    perror("custom mutator allocation (maybe_grow)");
    return 0; /* afl-fuzz will very likely error out after this. */
  }

  memcpy(mutated_out, tree->data_buf, mutated_size);
  *out_buf = mutated_out;
  return mutated_size;
}

// Save interesting mutated test cases
void afl_custom_queue_new_entry(my_mutator_t * data,
                                const uint8_t *filename_new_queue,
                                const uint8_t *filename_orig_queue) {
  // Skip if we read from initial test cases (i.e., from input directory)
  if (!filename_orig_queue) return;

  string fn((const char *)filename_new_queue);

  trees[fn] = data->mutated_tree;

  // Store all subtrees in the newly added tree
  chunk_store_add_tree(data->mutated_tree);

  /* Once the test case is added into the queue, we will clear `mutated_tree`
    pointer. In this case, we will store the tree instead destroying it in
    `afl_custom_fuzz`. */
  data->mutated_tree = nullptr;
}
