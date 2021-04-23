/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Written by Shengtuo Hu

   Copyright 2020 AFLplusplus Project. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   A grammar-based custom mutator written for GSoC '20.

 */

#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "helpers.h"
#include "tree.h"
#include "custom_mutator.h"
#include "f1_c_fuzz.h"
#include "tree_mutation.h"
#include "tree_trimming.h"
#include "chunk_store.h"
#include "utils.h"

my_mutator_t *afl_custom_init(afl_t *afl, unsigned int seed) {

  srandom(seed);

  chunk_store_init();

  my_mutator_t *data = (my_mutator_t *)calloc(1, sizeof(my_mutator_t));
  if (!data) {

    perror("custom mutator structure allocation error (afl_custom_init)");
    return NULL;

  }

  data->afl = afl;

  return data;

}

void afl_custom_deinit(my_mutator_t *data) {

  if (data->tree_cur) tree_free(data->tree_cur);
  if (data->mutated_tree) tree_free(data->mutated_tree);
  if (data->trimmed_tree) tree_free(data->trimmed_tree);

  data->cur_fuzzing_stage = 0;
  data->cur_fuzzing_step = 0;
  data->total_rules_mutation_steps = 0;
  data->total_random_mutation_steps = 0;
  data->total_random_recursive_mutation_steps = 0;
  data->total_splicing_mutation_steps = 0;

  data->cur_rules_mutation_node = NULL;
  data->cur_rules_mutation_rule_id = 0;

  data->cur_trimming_stage = 0;
  data->trim_was_effective = false;
  data->cur_subtree_trimming_step = 0;
  data->finished_subtree_trimming_nodes = 0;
  data->total_subtree_trimming_steps = 0;
  data->cur_recursive_trimming_step = 0;
  data->finished_recursive_trimming_edges = 0;
  data->total_recursive_trimming_steps = 0;

  free(data->fuzz_buf);
  free(data);

  chunk_store_clear();

}

// For each interesting test case in the queue
uint8_t afl_custom_queue_get(my_mutator_t *data, const uint8_t *filename) {

  const char *fn = (const char *)filename;
  data->filename_cur = filename;
  if (data->tree_cur) {

    // Clear the previous tree
    tree_free(data->tree_cur);

  }

  data->tree_cur = NULL;

  // Check the tree output directory
  if (unlikely(data->tree_out_dir_exist == 0)) {

    const char *last_slash = strrchr(fn, '/');
    if (unlikely(!last_slash)) {

      // Should not reach here
      perror("Invalid filename (afl_custom_queue_get)");
      return 0;

    }

    // Set the tree output directory
    char *tree_out_dir = strndup(fn, last_slash - fn);
    char *found = strstr(tree_out_dir, "/queue");
    if (unlikely(!found)) {

      // Should not reach here
      perror("Invalid filename (afl_custom_queue_get)");
      return 0;

    }

    // Replace "queue" with "trees"
    memcpy(found, "/trees", 6);

    // Check whether the directory exists
    if (!create_directory(tree_out_dir)) {

      // error
      perror("Cannot create the output directory (afl_custom_queue_get)");
      return 0;

    }

    free(tree_out_dir);

  }

  snprintf(data->tree_fn_cur, PATH_MAX - 1, "%s", fn);
  char *found = strstr(data->tree_fn_cur, "/queue/");
  if (unlikely(!found)) {

    // Should not reach here
    perror("Invalid filename (afl_custom_queue_get)");
    return 0;

  }

  // Replace "queue" with "trees"
  memcpy(found, "/trees", 6);

  // Read the corresponding serialized tree from file
  data->tree_cur = read_tree_from_file(data->tree_fn_cur);
  if (data->tree_cur) {

    // We already had this tree in the trees folder, so compute its size and then we're done!
    tree_get_size(data->tree_cur);
    return 1;

  }

  // try to parse the test case
  data->tree_cur = load_tree_from_test_case(fn);
  if (data->tree_cur) {

    // Now that we've parsed it, cache the info from this test case in
    // our trees folder and in the chunk store
    tree_get_size(data->tree_cur);
    write_tree_to_file(data->tree_cur, data->tree_fn_cur);
    chunk_store_add_tree(data->tree_cur);
    return 1;

  }

  // parsing error, skip the current test case

  return 0;

}

// Trimming
int32_t afl_custom_init_trim(my_mutator_t *                   data,
                             __attribute__((unused)) uint8_t *buf,
                             __attribute__((unused)) size_t   buf_size) {

  if (!data->tree_cur) return 0;

  tree_get_non_terminal_nodes(data->tree_cur);
  tree_get_recursion_edges(data->tree_cur);

  data->cur_trimming_stage = 0;
  data->trim_was_effective = false;

  data->cur_subtree_trimming_step = 0;
  data->finished_subtree_trimming_nodes = 0;
  data->total_subtree_trimming_steps = data->tree_cur->root->non_term_size;

  data->cur_recursive_trimming_step = 0;
  data->finished_recursive_trimming_edges = 0;
  data->total_recursive_trimming_steps =
      data->tree_cur->root->recursion_edge_size;

  return data->total_subtree_trimming_steps +
         data->total_recursive_trimming_steps;

}

size_t afl_custom_trim(my_mutator_t *data, uint8_t **out_buf) {

  tree_t *trimmed_tree = NULL;
  size_t  trimmed_size = 0;
  tree_t *tree_cur = data->tree_cur;

  if (data->cur_trimming_stage == 0) {

    // subtree trimming
    node_t *node = (node_t *)list_pop_front(tree_cur->non_terminal_node_list);
    trimmed_tree = subtree_trimming(tree_cur, node);

  } else if (data->cur_trimming_stage == 1) {

    // recursive trimming
    edge_t *edge = (edge_t *)list_pop_front(tree_cur->recursion_edge_list);
    trimmed_tree = recursive_trimming(tree_cur, *edge);
    free(edge);

  } else {

    // should not reach here
    *out_buf = NULL;
    perror("wrong trimming stage (afl_custom_trim)");
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

    *out_buf = NULL;
    perror("custom mutator allocation (maybe_grow)");
    return 0;            /* afl-fuzz will very likely error out after this. */

  }

  memcpy(trimmed_out, trimmed_tree->data_buf, trimmed_size);
  *out_buf = trimmed_out;

  return trimmed_size;

}

int32_t afl_custom_post_trim(my_mutator_t *data, int success) {

  if (success) {
    // Keep track that we will need to rewrite the tree file later
    data->trim_was_effective = true;

    // Swap in the trimmed tree as our current tree:
    tree_free(data->tree_cur);
    data->tree_cur = data->trimmed_tree;

    // Update the non-terminal node list
    tree_get_non_terminal_nodes(data->tree_cur);

    // The idea here is that the results that we've already tried are always at
    // the start of the non_terminal_node_list, because whenever we get a "yes" result
    // the nodes get trimmed out and they are always the last thing we've popped off the list.
    //
    // So to avoid repeating the previous attempts, just pop them out of the list right away:
    for (uint32_t i = 0; i < data->finished_subtree_trimming_nodes; ++i)
      list_pop_front(data->tree_cur->non_terminal_node_list);

    // We are now pointing *at* the node we just replaced.
    // So we want to skip it and its children!
    node_t * node = list_pop_front(data->tree_cur->non_terminal_node_list);
    if (node) {

      // Remember to skip these next time as well:
      data->finished_subtree_trimming_nodes += 1 + node->non_term_size;

      for (uint32_t i = 0; i < node->non_term_size; ++i)
        list_pop_front(data->tree_cur->non_terminal_node_list);

    }

    size_t new_node_size = data->tree_cur->non_terminal_node_list->size;

    // Set our progress equal to the remaining un-tested nodes in the list:
    data->cur_subtree_trimming_step = data->total_subtree_trimming_steps - new_node_size;


    // update the recursion edge list
    tree_get_recursion_edges(data->tree_cur);
    // Avoid repeating work by skipping edges that we have already tried.
    for (uint32_t i = 0; i < data->finished_recursive_trimming_edges; ++i)
      list_pop_front(data->tree_cur->recursion_edge_list);
    size_t new_edge_size = data->tree_cur->recursion_edge_list->size;

    // Set our progress equal to the remaining un-tested edges in the list:
    data->cur_recursive_trimming_step = data->total_recursive_trimming_steps - new_edge_size;

  } else {

    // the trimmed tree will not be saved, so destroy it
    tree_free(data->trimmed_tree);

    // Even if the trim didn't work, still count the progress!
    if (data->cur_trimming_stage == 0) {

      ++data->cur_subtree_trimming_step;
      ++data->finished_subtree_trimming_nodes;

    }

    if (data->cur_trimming_stage == 1) {

      ++data->cur_recursive_trimming_step;
      ++data->finished_recursive_trimming_edges;

    }

  }

  data->trimmed_tree = NULL;

  // update trimming stage
  if (data->cur_trimming_stage == 0) {

    if (data->cur_subtree_trimming_step >= data->total_subtree_trimming_steps)
      ++data->cur_trimming_stage;

  }

  if (data->cur_trimming_stage == 1) {

    if (data->cur_recursive_trimming_step >=
        data->total_recursive_trimming_steps)
      ++data->cur_trimming_stage;

  }

  // If we're done and we reduced the tree, then save the tree back to the
  // file and write it to the chunk store for use in future splice mutations:
  if (data->trim_was_effective && data->cur_trimming_stage > 1) {

    // Update the corresponding tree file
    write_tree_to_file(data->tree_cur, data->tree_fn_cur);
    chunk_store_add_tree(data->tree_cur);

  }

  return data->cur_subtree_trimming_step + data->cur_recursive_trimming_step;

}

uint32_t afl_custom_fuzz_count(my_mutator_t *                         data,
                               __attribute__((unused)) const uint8_t *buf,
                               __attribute__((unused)) size_t buf_size) {

  if (!data->tree_cur) return 0;

  tree_get_non_terminal_nodes(data->tree_cur);
  tree_get_recursion_edges(data->tree_cur);

  data->cur_fuzzing_stage = 0;
  data->cur_fuzzing_step = 0;
  data->total_rules_mutation_steps = rules_mutation_count(data->tree_cur);
  data->total_random_mutation_steps = 100;
  if (data->tree_cur->recursion_edge_list->size > 0) {

    data->total_random_recursive_mutation_steps = 20;

  } else {

    data->total_random_recursive_mutation_steps = 0;

  }

  data->total_splicing_mutation_steps = 100;

  if (data->total_rules_mutation_steps > 0) {

    // find `node` and `rule_id`
    data->cur_rules_mutation_node =
        (node_t *)list_pop_front(data->tree_cur->non_terminal_node_list);
    data->cur_rules_mutation_rule_id = 0;

    size_t num_rules = 0;
    while (true) {

      num_rules = node_num_rules[data->cur_rules_mutation_node->id];
      if (data->cur_rules_mutation_rule_id >= num_rules) {

        // next node
        data->cur_rules_mutation_node =
            (node_t *)list_pop_front(data->tree_cur->non_terminal_node_list);
        // next rule id
        data->cur_rules_mutation_rule_id = 0;

      }

      if (data->cur_rules_mutation_rule_id !=
          data->cur_rules_mutation_node->rule_id)
        break;

      // skip the current rule id
      ++data->cur_rules_mutation_rule_id;

    }

  } else {

    // no rules mutation stage
    // move to next stage
    ++data->cur_fuzzing_stage;
    data->cur_fuzzing_step = 0;

  }

  return data->total_rules_mutation_steps + data->total_random_mutation_steps +
         data->total_random_recursive_mutation_steps +
         data->total_splicing_mutation_steps;

}

// Fuzz the given test case several times, which is defined by the
// `custom_mutator_stage` in `afl-fuzz-one.c`
size_t afl_custom_fuzz(my_mutator_t *data, __attribute__((unused)) uint8_t *buf,
                       __attribute__((unused)) size_t   buf_size,
                       uint8_t **                       out_buf,
                       __attribute__((unused)) uint8_t *add_buf,
                       __attribute__((unused)) size_t   add_buf_size,
                       size_t                           max_size) {

  tree_t *tree = NULL;
  size_t  mutated_size = 0;

  if (data->mutated_tree) {

    /* `data->mutated_tree` is not NULL, meaning that this is not an interesting
      mutation (`afl_custom_queue_new_entry` is not invoked). Therefore, we
      need to free the memory. */
    tree_free(data->mutated_tree);
    data->mutated_tree = NULL;

  }

  tree = data->tree_cur;
  if (unlikely(!tree)) {

    // Randomly generate a test case
    tree = gen_init__(500);
    tree_get_non_terminal_nodes(tree);
    tree_get_size(tree);

  }

  switch (data->cur_fuzzing_stage) {

    case 0:
      // rules mutation
      tree = rules_mutation(tree, data->cur_rules_mutation_node,
                            data->cur_rules_mutation_rule_id);
      break;
    case 1:
      // random mutation
      tree = random_mutation(tree);
      break;
    case 2:
      // random recursive mutation
      tree = random_recursive_mutation(tree, random() % 10);
      break;
    case 3:
      // splicing mutation
      tree = splicing_mutation(tree);
      break;
    default:
      perror("mutation error, invalid choice (afl_custom_fuzz)");
      return 0;

  }

  if (!tree) {

    perror("mutation error, empty tree (afl_custom_fuzz)");
    return 0;

  }

  // update internal status
  ++data->cur_fuzzing_step;
  if (data->cur_fuzzing_stage == 0) {

    // rules mutation
    if (data->cur_fuzzing_step >= data->total_rules_mutation_steps) {

      ++data->cur_fuzzing_stage;
      data->cur_fuzzing_step = 0;

    } else {

      // update node and rule id
      ++data->cur_rules_mutation_rule_id;  // next rule id
      size_t num_rules = 0;
      while (true) {

        num_rules = node_num_rules[data->cur_rules_mutation_node->id];
        if (data->cur_rules_mutation_rule_id >= num_rules) {

          // next node
          data->cur_rules_mutation_node =
              (node_t *)list_pop_front(data->tree_cur->non_terminal_node_list);
          // next rule id
          data->cur_rules_mutation_rule_id = 0;

        }

        if (data->cur_rules_mutation_rule_id !=
            data->cur_rules_mutation_node->rule_id)
          break;

        // skip the current rule id
        ++data->cur_rules_mutation_rule_id;

      }

    }

  }

  if (data->cur_fuzzing_stage == 1) {

    // random mutation
    if (data->cur_fuzzing_step >= data->total_random_mutation_steps) {

      ++data->cur_fuzzing_stage;
      data->cur_fuzzing_step = 0;

    }

  }

  if (data->cur_fuzzing_stage == 2) {

    // random recursive mutation
    if (data->cur_fuzzing_step >= data->total_random_recursive_mutation_steps) {

      ++data->cur_fuzzing_stage;
      data->cur_fuzzing_step = 0;

    }

  }

  if (data->cur_fuzzing_stage == 3) {

    // splicing mutation
    if (data->cur_fuzzing_step >= data->total_splicing_mutation_steps) {

      ++data->cur_fuzzing_stage;
      data->cur_fuzzing_step = 0;

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

    *out_buf = NULL;
    perror("custom mutator, fuzzing buffer allocation error (afl_custom_fuzz)");
    return 0;            /* afl-fuzz will very likely error out after this. */

  }

  memcpy(mutated_out, tree->data_buf, mutated_size);
  *out_buf = mutated_out;
  return mutated_size;

}

// Save interesting mutated test cases
void afl_custom_queue_new_entry(my_mutator_t * data,
                                const uint8_t *filename_new_queue,
                                const uint8_t *filename_orig_queue) {

  // If this is an initial case or sync, then we will get called with a null "filename_orig_queue".
  if (unlikely(!filename_orig_queue || !data->mutated_tree)) {

    // In that situation, we can skip it here and let afl_custom_queue_get() import the data later,
    // or we can prefetch it here to ensure that it gets into our splicing data set (chunk_store) asap.
    // Choosing the second option for now, but if this is inefficient we can just return instead of
    // calling afl_custom_queue_get().
    afl_custom_queue_get(data, filename_new_queue);
    return;

  }

  const char *fn = (const char *)filename_new_queue;
  snprintf(data->new_tree_fn, PATH_MAX - 1, "%s", fn);
  char *found = strstr(data->new_tree_fn, "/queue/");
  if (unlikely(!found)) {

    // Should not reach here
    fprintf(stderr, "Invalid filename_new_queue (afl_custom_queue_new_entry)\n");
    return;

  }

  // Replace "queue" with "trees"
  memcpy(found, "/trees", 6);

  // Write the mutated tree to the file
  write_tree_to_file(data->mutated_tree, data->new_tree_fn);

  // Store all subtrees in the newly added tree
  chunk_store_add_tree(data->mutated_tree);

  /* Once the test case is added into the queue, we will clear `mutated_tree` */
  tree_free(data->mutated_tree);
  data->mutated_tree = NULL;

}
