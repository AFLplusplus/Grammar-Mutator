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
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "helpers.h"
#include "tree.h"
#include "custom_mutator.h"
#include "f1_c_fuzz.h"
#include "tree_mutation.h"
#include "tree_trimming.h"
#include "chunk_store.h"
#include "utils.h"

// default number of mutations of three mutation strategies
// env: RANDOM_MUTATION_STEPS
size_t default_random_mutation_steps = 1000;
// env: RANDOM_RECURSIVE_MUTATION_STEPS
size_t default_random_recursive_mutation_steps = 1000;
// env: SPLICING_MUTATION_STEPS
size_t default_splicing_mutation_steps = 1000;
// Maximum rules mutations per entry; unlimited by default.
// env: RULES_MUTATION_STEPS
size_t default_rules_mutation_steps = SIZE_MAX;
// Maximum trimming steps per entry; unlimited by default.
// env: TRIMMING_STEPS
size_t default_trimming_steps = SIZE_MAX;

// Use the parse time budget unless PARSE_TIME_BUDGET is exactly "0".
// env: PARSE_TIME_BUDGET
static bool parse_time_budget_on = true;

static void load_env_configs() {

  char *ptr;
  char *env_vars[6] = {

      "RANDOM_MUTATION_STEPS",   "RANDOM_RECURSIVE_MUTATION_STEPS",
      "SPLICING_MUTATION_STEPS", "RULES_MUTATION_STEPS",
      "TRIMMING_STEPS",          NULL};
  size_t *configs[6] = {&default_random_mutation_steps,
                        &default_random_recursive_mutation_steps,
                        &default_splicing_mutation_steps,
                        &default_rules_mutation_steps,
                        &default_trimming_steps,
                        NULL};
  int     i = 0;

  while (env_vars[i] != NULL && configs[i] != NULL) {

    ptr = getenv(env_vars[i]);
    if (ptr && *ptr) { *(configs[i]) = strtol(ptr, NULL, 10); }
    ++i;

  }

  ptr = getenv("PARSE_TIME_BUDGET");
  parse_time_budget_on = !(ptr && strcmp(ptr, "0") == 0);

}

my_mutator_t *afl_custom_init(afl_t *afl, unsigned int seed) {

  random_set_seed(seed);

  load_env_configs();

  chunk_store_init();

  my_mutator_t *data = (my_mutator_t *)calloc(1, sizeof(my_mutator_t));
  if (!data) {

    perror("custom mutator structure allocation error (afl_custom_init)");
    return NULL;

  }

  data->afl = afl;
  data->described_stage = UINT8_MAX;
  map_init(&data->rejected);

  if (parse_time_budget_on) {

    data->budget = parse_budget_create();
    // Without a budget, the parser runs without a time limit
    if (!data->budget)
      perror("Cannot create the parse budget (afl_custom_init)");

  }

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

  parse_budget_free(data->budget);
  map_deinit(&data->rejected);

  free(data->fuzz_buf);
  free(data);

  chunk_store_clear();

}

/* Parse the test case file "fn". Return NULL if the parser rejected the entry
  before, or if the parse fails.

  The load function leaves the status as PARSE_OK when it cannot read the file.
  So NULL with PARSE_OK means "not read". The code does not remember such an
  entry. It does not remember PARSE_THREAD_ERROR either, because a retry can
  work. Any other failure goes into the rejected set for the rest of the run. */
static tree_t *parse_entry(my_mutator_t *data, const char *fn) {

  if (map_get(&data->rejected, fn)) return NULL;

  parse_status_t status = PARSE_OK;
  tree_t        *tree = load_tree_from_test_case_ex(fn, data->budget, &status);
  if (tree) return tree;

  // Print each warning once per run, not once per entry.
  static bool warned_thread = false, warned_rejected = false;

  if (status == PARSE_THREAD_ERROR && !warned_thread) {

    warned_thread = true;
    fprintf(stderr,
            "[grammar mutator] warning: the parse thread cannot start. "
            "The mutator does not parse test cases that have no tree file.\n");

  }

  if (status != PARSE_OK && status != PARSE_THREAD_ERROR &&
      map_set(&data->rejected, fn, 1) != 0 && !warned_rejected) {

    // The set is out of memory. The entry stays unmarked, so the next call
    // parses it again. This costs time only.
    warned_rejected = true;
    fprintf(stderr,
            "[grammar mutator] warning: out of memory. The mutator parses a "
            "rejected test case again.\n");

  }

  return NULL;

}

/* Return true if the file "fn" holds exactly the "len" bytes of "buf" */
static bool file_has_bytes(const char *fn, const uint8_t *buf, size_t len) {

  int fd = open(fn, O_RDONLY);
  if (fd < 0) return false;

  struct stat info;
  bool        same = !fstat(fd, &info) && (uint64_t)info.st_size == len;
  uint8_t     chunk[4096];
  size_t      done = 0;
  while (same && done < len) {

    size_t  want = len - done < sizeof(chunk) ? len - done : sizeof(chunk);
    ssize_t got = read(fd, chunk, want);
    if (got <= 0 || memcmp(chunk, buf + done, (size_t)got))
      same = false;
    else
      done += (size_t)got;

  }

  close(fd);
  return same;

}

/* Return true if the bytes of "tree" are the bytes of the file "fn" */
static bool tree_gives_file(tree_t *tree, const char *fn) {

  return tree_to_buf(tree) &&
         file_has_bytes(fn, tree->data_buf, tree->data_len);

}

/* Set "tree_fn" (a PATH_MAX buffer) to the tree file of the queue entry "fn".
  The tree file is in the "trees" folder next to the "queue" or "_resume"
  folder of the entry. Create the "trees" folder if it does not exist. Set an
  empty name if the entry is in another folder. Return false on an error. */
static bool get_tree_fn(const char *fn, char *tree_fn) {

  tree_fn[0] = '\0';

  // Strip off the file portion of the filename
  const char *slash_basename = strrchr(fn, '/');
  if (unlikely(!slash_basename)) {

    // Should not reach here
    fprintf(stderr, "No folder in filename (get_tree_fn)\n");
    return false;

  }

  // At the initialization stage of afl-fuzz, there will be an "orig:" tag in
  // the filename. If so, we will look for dumped tree files with the same name,
  // as our grammar generator will assign the same name for generated test cases
  // and trees. If there is no "orig:" tag, we will use its full name of the
  // test case
  const char *use_name = strstr(slash_basename, ",orig:");
  if (use_name) {

    // Skip the prefix ",orig:"
    use_name += 6;

  } else {

    // Skip the prefix slash
    use_name = slash_basename + 1;

  }

  char *tree_out_dir = strdup(fn);
  tree_out_dir[slash_basename - fn] = '\0';

  // Get the name of the parent folder
  char *last_dir = strrchr(tree_out_dir, '/');
  if (unlikely(!last_dir)) {

    // Should not reach here
    fprintf(stderr, "No parent folder in filename (get_tree_fn)\n");
    free(tree_out_dir);
    return false;

  }

  // Only read or write trees if we get /queue or /_resume as the last folder!
  if (strcmp(last_dir, "/queue") != 0 && strcmp(last_dir, "/_resume") != 0) {

    free(tree_out_dir);
    return true;

  }

  // Copy "/trees" (including the null) to replace the old folder name
  memcpy(last_dir, "/trees", 7);

  if (!create_directory(tree_out_dir)) {

    perror("Cannot create the output directory (get_tree_fn)");
    free(tree_out_dir);
    return false;

  }

  snprintf(tree_fn, PATH_MAX - 1, "%s/%s", tree_out_dir, use_name);
  free(tree_out_dir);
  return true;

}

/* Get the tree of the queue entry "fn". Read the tree file "tree_fn". With
  "check_bytes", use the tree file only if its tree gives the bytes of the
  entry. Without a usable tree file, parse the entry and write its tree file.
  An empty "tree_fn" means no tree file. Return NULL if the parser rejects the
  entry. */
static tree_t *load_entry_tree(my_mutator_t *data, const char *fn,
                               const char *tree_fn, bool check_bytes) {

  tree_t *tree = NULL;
  if (tree_fn[0]) tree = read_tree_from_file(tree_fn);
  if (tree && check_bytes && !tree_gives_file(tree, fn)) {

    tree_free(tree);
    tree = NULL;

  }

  if (tree) {

    tree_get_size(tree);
    return tree;

  }

  tree = parse_entry(data, fn);
  if (!tree) return NULL;

  tree_get_size(tree);
  if (tree_fn[0]) write_tree_to_file(tree, tree_fn);
  return tree;

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
  data->described_stage = UINT8_MAX;

  if (!get_tree_fn(fn, data->tree_fn_cur)) return 0;

  // Add the subtrees of the entry to the chunk store. If the parser rejected
  // the entry, keep it in the queue without a tree, so that the other AFL++
  // stages still fuzz it.
  data->tree_cur = load_entry_tree(data, fn, data->tree_fn_cur, false);
  if (data->tree_cur) chunk_store_add_tree(data->tree_cur);
  return 1;

}

// Trimming
int32_t afl_custom_init_trim(my_mutator_t                    *data,
                             __attribute__((unused)) uint8_t *buf,
                             __attribute__((unused)) size_t   buf_size) {

  /* `data->mutated_tree` comes from the fuzzing stage of the previous queue
    entry, so it is stale now. AFL++ 5.03c and later can call
    `afl_custom_queue_new_entry` during trimming. That function must not save
    the stale tree for a trimmed test case. */
  if (data->mutated_tree) {

    tree_free(data->mutated_tree);
    data->mutated_tree = NULL;

  }

  if (!data->tree_cur || !default_trimming_steps) return 0;

  // Without the full lists, do no trimming
  if (!tree_get_non_terminal_nodes(data->tree_cur) ||
      !tree_get_recursion_edges(data->tree_cur))
    return 0;

  data->cur_trimming_stage = 0;
  data->trim_was_effective = false;
  data->trimming_steps = 0;

  // Take the step counts from the lists, not from the node sizes. Then each
  // step has a node or an edge, also if the sizes are stale.
  data->cur_subtree_trimming_step = 0;
  data->finished_subtree_trimming_nodes = 0;
  data->total_subtree_trimming_steps =
      data->tree_cur->non_terminal_node_list->size;

  data->cur_recursive_trimming_step = 0;
  data->finished_recursive_trimming_edges = 0;
  data->total_recursive_trimming_steps =
      data->tree_cur->recursion_edge_list->size;

  return data->total_subtree_trimming_steps +
         data->total_recursive_trimming_steps;

}

size_t afl_custom_trim(my_mutator_t *data, uint8_t **out_buf) {

  tree_t *trimmed_tree = NULL;
  size_t  trimmed_size = 0;
  tree_t *tree_cur = data->tree_cur;

  if (unlikely(!tree_cur)) {

    // No tree, so there is nothing to trim
    *out_buf = NULL;
    return 0;

  }

  if (data->cur_trimming_stage == 0) {

    // subtree trimming
    node_t *node = (node_t *)list_pop_front(tree_cur->non_terminal_node_list);
    if (unlikely(!node)) {

      // should not reach here: the step count is the list size
      *out_buf = NULL;
      fprintf(stderr, "no node left to trim (afl_custom_trim)\n");
      return 0;

    }

    trimmed_tree = subtree_trimming(tree_cur, node);

  } else if (data->cur_trimming_stage == 1) {

    // recursive trimming
    edge_t *edge = (edge_t *)list_pop_front(tree_cur->recursion_edge_list);
    if (unlikely(!edge)) {

      // should not reach here: the step count is the list size
      *out_buf = NULL;
      fprintf(stderr, "no edge left to trim (afl_custom_trim)\n");
      return 0;

    }

    trimmed_tree = recursive_trimming(tree_cur, *edge);
    free(edge);

  } else {

    // should not reach here
    *out_buf = NULL;
    fprintf(stderr, "wrong trimming stage (afl_custom_trim)\n");
    return 0;

  }

  if (unlikely(!trimmed_tree)) {

    *out_buf = NULL;
    fprintf(stderr, "trimming error, empty tree (afl_custom_trim)\n");
    return 0;

  }

  if (unlikely(!tree_to_buf(trimmed_tree))) {

    tree_free(trimmed_tree);
    *out_buf = NULL;
    return 0;

  }

  tree_get_size(trimmed_tree);
  data->trimmed_tree = trimmed_tree;

  // maybe_grow is optimized to be quick for reused buffers.
  trimmed_size = trimmed_tree->data_len;
  uint8_t *trimmed_out =
      (uint8_t *)maybe_grow(BUF_PARAMS(data, fuzz), trimmed_size);
  if (!trimmed_out) {

    *out_buf = NULL;
    fprintf(stderr, "custom mutator allocation (maybe_grow)\n");
    return 0;            /* afl-fuzz will very likely error out after this. */

  }

  memcpy(trimmed_out, trimmed_tree->data_buf, trimmed_size);
  *out_buf = trimmed_out;

  return trimmed_size;

}

// After a successful trim, remove the tried nodes and edges from the new lists
// and set the progress to match the lists
static void skip_tried_trimming_steps(my_mutator_t *data) {

  // The idea here is that the results that we've already tried are always at
  // the start of the non_terminal_node_list, because whenever we get a "yes"
  // result the nodes get trimmed out and they are always the last thing we've
  // popped off the list.
  //
  // So to avoid repeating the previous attempts, just pop them out of the
  // list right away:
  for (uint32_t i = 0; i < data->finished_subtree_trimming_nodes; ++i)
    list_pop_front(data->tree_cur->non_terminal_node_list);

  // We are now pointing *at* the node we just replaced.
  // So we want to skip it and its children!
  node_t *node = list_pop_front(data->tree_cur->non_terminal_node_list);
  if (node) {

    // Remember to skip these next time as well:
    data->finished_subtree_trimming_nodes += 1 + node->non_term_size;

    for (uint32_t i = 0; i < node->non_term_size; ++i)
      list_pop_front(data->tree_cur->non_terminal_node_list);

  }

  size_t new_node_size = data->tree_cur->non_terminal_node_list->size;

  // Set our progress equal to the remaining un-tested nodes in the list. The
  // remaining steps must not be more than the list size.
  data->cur_subtree_trimming_step =
      new_node_size < data->total_subtree_trimming_steps
          ? data->total_subtree_trimming_steps - new_node_size
          : 0;

  // Avoid repeating work by skipping edges that we have already tried. The
  // list owns its edges, so free them.
  for (uint32_t i = 0; i < data->finished_recursive_trimming_edges; ++i)
    free(list_pop_front(data->tree_cur->recursion_edge_list));
  size_t new_edge_size = data->tree_cur->recursion_edge_list->size;

  // Set our progress equal to the remaining un-tested edges in the list:
  data->cur_recursive_trimming_step =
      new_edge_size < data->total_recursive_trimming_steps
          ? data->total_recursive_trimming_steps - new_edge_size
          : 0;

}

int32_t afl_custom_post_trim(my_mutator_t *data, int success) {

  if (unlikely(!data->tree_cur)) return 0;

  if (success) {

    // Keep track that we will need to rewrite the tree file later
    data->trim_was_effective = true;

    // Swap in the trimmed tree as our current tree:
    tree_free(data->tree_cur);
    data->tree_cur = data->trimmed_tree;

    // Update the lists. Without the full lists, stop the trimming.
    if (tree_get_non_terminal_nodes(data->tree_cur) &&
        tree_get_recursion_edges(data->tree_cur))
      skip_tried_trimming_steps(data);
    else
      data->cur_trimming_stage = 2;

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

  // Stop after TRIMMING_STEPS steps, keeping what the trimming reached
  if (++data->trimming_steps >= default_trimming_steps)
    data->cur_trimming_stage = 2;

  // If we're done and we reduced the tree, then save the tree back to the
  // file and write it to the chunk store for use in future splice mutations:
  if (data->trim_was_effective && data->cur_trimming_stage > 1) {

    // Update the corresponding tree file
    write_tree_to_file(data->tree_cur, data->tree_fn_cur);
    chunk_store_add_tree(data->tree_cur);

  }

  if (data->cur_trimming_stage > 1)
    return data->total_subtree_trimming_steps +
           data->total_recursive_trimming_steps;

  return data->cur_subtree_trimming_step + data->cur_recursive_trimming_step;

}

// Take this alternative rule with probability needed / left.
static bool select_rules_mutation_pair(my_mutator_t *data) {

  size_t left = data->rules_mutation_pairs_left--;
  size_t needed = data->total_rules_mutation_steps - data->cur_fuzzing_step;
  return needed >= left ||
         random_below(left < UINT32_MAX ? left : UINT32_MAX) < needed;

}

/* Move to the next node and rule id of the rules mutation stage, from the
  current rule id on. Return false if the node list has no next pair. The
  caller then ends the stage. */
static bool next_rules_mutation_pair(my_mutator_t *data) {

  while (true) {

    node_t *node = data->cur_rules_mutation_node;
    if (!node || data->cur_rules_mutation_rule_id >= node_num_rules[node->id]) {

      // next node
      node = (node_t *)list_pop_front(data->tree_cur->non_terminal_node_list);
      if (unlikely(!node)) return false;
      data->cur_rules_mutation_node = node;
      // next rule id
      data->cur_rules_mutation_rule_id = 0;

    }

    if (data->cur_rules_mutation_rule_id != node->rule_id &&
        select_rules_mutation_pair(data))
      return true;

    // skip the current rule id
    ++data->cur_rules_mutation_rule_id;

  }

}

uint32_t afl_custom_fuzz_count(my_mutator_t                          *data,
                               __attribute__((unused)) const uint8_t *buf,
                               __attribute__((unused)) size_t buf_size) {

  if (!data->tree_cur) return 0;

  /* Compute the sizes again: the random and splicing stages pick a node with
    them. A terminal root has no node to mutate. If a walk runs out of memory,
    the sizes or the lists are not complete. In these cases, plan no mutation.
  */
  if (!tree_get_size(data->tree_cur) ||
      !tree_get_non_terminal_nodes(data->tree_cur) ||
      !tree_get_recursion_edges(data->tree_cur))
    return 0;

  data->cur_fuzzing_stage = 0;
  data->cur_fuzzing_step = 0;
  data->total_rules_mutation_steps = rules_mutation_count(data->tree_cur);
  data->rules_mutation_pairs_left = data->total_rules_mutation_steps;
  if (data->total_rules_mutation_steps > default_rules_mutation_steps)
    data->total_rules_mutation_steps = default_rules_mutation_steps;
  data->total_random_mutation_steps = default_random_mutation_steps;
  if (data->tree_cur->recursion_edge_list->size > 0) {

    data->total_random_recursive_mutation_steps =
        default_random_recursive_mutation_steps;

  } else {

    data->total_random_recursive_mutation_steps = 0;

  }

  data->total_splicing_mutation_steps = default_splicing_mutation_steps;

  // find `node` and `rule_id`
  data->cur_rules_mutation_node = NULL;
  data->cur_rules_mutation_rule_id = 0;
  if (data->total_rules_mutation_steps > 0 && !next_rules_mutation_pair(data))
    data->total_rules_mutation_steps = 0;

  if (data->total_rules_mutation_steps == 0) {

    // no rules mutation stage
    // move to next stage
    ++data->cur_fuzzing_stage;
    data->cur_fuzzing_step = 0;

  }

  return data->total_rules_mutation_steps + data->total_random_mutation_steps +
         data->total_random_recursive_mutation_steps +
         data->total_splicing_mutation_steps;

}

// AFL++ only checks that this symbol exists. It then does not load a second
// test case (`add_buf`) for each `afl_custom_fuzz` call. The mutator does not
// use `add_buf`, because the splicing mutation reads the chunk store.
void afl_custom_splice_optout(__attribute__((unused)) my_mutator_t *data) {

}

// The labels of the stages 0 to 3 in the file names of the found cases
static const char *const stage_labels[] = {"op:gm-rules", "op:gm-random",
                                           "op:gm-recursive", "op:gm-splice"};

// AFL++ adds the result to the name of a case that the last output found, in
// `queue/`, `crashes/`, or `hangs/`. Return NULL if there is no label. AFL++
// then uses its own text.
const char *afl_custom_describe(my_mutator_t *data,
                                size_t        max_description_len) {

  uint8_t stage = data->described_stage;
  if (stage >= sizeof(stage_labels) / sizeof(stage_labels[0]) ||
      max_description_len == 0)
    return NULL;

  size_t size = max_description_len < sizeof(data->description)
                    ? max_description_len
                    : sizeof(data->description);
  snprintf(data->description, size, "%s", stage_labels[stage]);
  return data->description;

}

// Fuzz the given test case several times, which is defined by the
// `custom_mutator_stage` in `afl-fuzz-one.c`
size_t afl_custom_fuzz(my_mutator_t *data, __attribute__((unused)) uint8_t *buf,
                       __attribute__((unused)) size_t   buf_size,
                       uint8_t                        **out_buf,
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

    // The entry has no tree, and the mutation stages need one
    *out_buf = NULL;
    return 0;

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
    case 2: {

      // random recursive mutation
      const unsigned RRM_GROWTH =
          10;  // Allow 2**RRM_GROWTH of bytes of expansion
      tree_t *rrm_tree = NULL;
      int     failed_count = 8;
      do {

        if (failed_count-- <= 0) { break; }

        // Make sure that the mutation doesn't grow more than RRM_GROWTH bytes
        // per attempt! This is protecting against random_recursive_mutation's
        // ability to create MASSIVE growth in a short period of time by
        // duplicating big nodes.
        rrm_tree = random_recursive_mutation(tree, random_below(RRM_GROWTH + 1),
                                             1 << RRM_GROWTH);

      } while (!rrm_tree);

      // If every attempt grew too much, the tree stays as it was.
      tree = rrm_tree ? rrm_tree : tree_clone(tree);
      break;

    }

    case 3:
      // splicing mutation
      tree = splicing_mutation(tree);
      break;
    default:
      // All stages are done. AFL++ calls no more than the planned steps, so
      // this occurs only if the rules mutation stage ended early.
      *out_buf = NULL;
      fprintf(stderr, "mutation error, invalid choice (afl_custom_fuzz)\n");
      return 0;

  }

  if (!tree) {

    *out_buf = NULL;
    fprintf(stderr, "mutation error, empty tree (afl_custom_fuzz)\n");
    return 0;

  }

  // Keep the stage of this output for `afl_custom_describe`. The code below
  // can move to the next stage before AFL++ asks for the label.
  data->described_stage = data->cur_fuzzing_stage;

  // update internal status
  ++data->cur_fuzzing_step;
  if (data->cur_fuzzing_stage == 0) {

    // rules mutation
    if (data->cur_fuzzing_step >= data->total_rules_mutation_steps) {

      ++data->cur_fuzzing_stage;
      data->cur_fuzzing_step = 0;

    } else {

      // update node and rule id. Without a next pair, end the stage.
      ++data->cur_rules_mutation_rule_id;  // next rule id
      if (!next_rules_mutation_pair(data)) {

        ++data->cur_fuzzing_stage;
        data->cur_fuzzing_step = 0;

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

  if (unlikely(!tree_to_buf(tree))) {

    tree_free(tree);
    *out_buf = NULL;
    return 0;

  }

  // The output tree keeps the node sizes of `tree_cur`, which can be stale.
  // Nothing reads them: a tree gets new sizes when it becomes `tree_cur`.
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
uint8_t afl_custom_queue_new_entry(my_mutator_t  *data,
                                   const uint8_t *filename_new_queue,
                                   __attribute__((unused))
                                   const uint8_t *filename_orig_queue) {

  /* AFL++ gives no original file name for an initial entry, for a find while
    no entry is current, and for a find in an IJON round (AFL++ 5.03c and
    later). So the code does not use it. The code never changes
    `data->tree_cur`, because the fuzz stage or the trim loop can still use it
    (issue #57). It loads the new entry into a separate tree. */
  const char *fn = (const char *)filename_new_queue;

  /* `data->mutated_tree` is the last output of `afl_custom_fuzz`. It is the
    tree of the new entry only if it gives the bytes of the queued file.
    Another AFL++ stage (for example cmplog or havoc) can find the entry, and
    `max_size` can cut the output. `afl_custom_fuzz` built the bytes of this
    tree, and nothing changed the tree after that, so use them. */
  tree_t *tree = data->mutated_tree;
  data->mutated_tree = NULL;
  if (tree && !file_has_bytes(fn, tree->data_buf, tree->data_len)) {

    tree_free(tree);
    tree = NULL;

  }

  if (unlikely(!get_tree_fn(fn, data->new_tree_fn))) {

    tree_free(tree);
    return 0;

  }

  if (tree) {

    if (data->new_tree_fn[0]) write_tree_to_file(tree, data->new_tree_fn);

  } else {

    /* Read the tree file (for example of a seed), or parse the entry. The
      trees folder can hold a file with this name from an old run in the same
      output folder, so check its bytes. */
    tree = load_entry_tree(data, fn, data->new_tree_fn, true);
    if (unlikely(!tree)) return 0;

  }

  // Store all subtrees in the newly added tree
  chunk_store_add_tree(tree);
  tree_free(tree);

  // The queue file is not changed
  return 0;

}
