#ifndef __CUSTOM_MUTATOR_H__
#define __CUSTOM_MUTATOR_H__

#ifndef _LARGEFILE64_SOURCE
  #define _LARGEFILE64_SOURCE
#endif

#define _FILE_OFFSET_BITS 64

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <limits.h>

#include "helpers.h"
#include "tree.h"
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

// default number of mutations of three mutation strategies
extern size_t default_random_mutation_steps;
extern size_t default_random_recursive_mutation_steps;
extern size_t default_splicing_mutation_steps;

typedef struct afl {

} afl_t;

typedef struct my_mutator {

  afl_t *afl;

  bool tree_out_dir_exist;

  const uint8_t *filename_cur;
  tree_t *       tree_cur;
  tree_t *       mutated_tree;
  tree_t *       trimmed_tree;

  // Trimming
  uint8_t cur_trimming_stage;  // 0: subtree trimming
                               // 1: recursive trimming

  bool trim_was_effective; // Did we change the file *at all* during trim?

  size_t cur_subtree_trimming_step;
  size_t finished_subtree_trimming_nodes;
  size_t total_subtree_trimming_steps;

  size_t cur_recursive_trimming_step;
  size_t finished_recursive_trimming_edges;
  size_t total_recursive_trimming_steps;

  // Fuzzing
  uint8_t cur_fuzzing_stage;  // 0: rules mutation
                              // 1: random mutation (100 times)
                              // 2: random recursive mutation (20 times)
                              // 3: splicing mutation (100 times)

  size_t cur_fuzzing_step;
  size_t total_rules_mutation_steps;
  size_t total_random_mutation_steps;
  size_t total_random_recursive_mutation_steps;
  size_t total_splicing_mutation_steps;

  // Rules mutation
  node_t * cur_rules_mutation_node;
  uint32_t cur_rules_mutation_rule_id;

  // Reused buffers:
  BUF_VAR(uint8_t, fuzz);

  // Tree output directory
  char tree_fn_cur[PATH_MAX];
  char new_tree_fn[PATH_MAX];

} my_mutator_t;

my_mutator_t *afl_custom_init(afl_t *afl, unsigned int seed);
void          afl_custom_deinit(my_mutator_t *data);

uint8_t afl_custom_queue_get(my_mutator_t *data, const uint8_t *filename);

int32_t afl_custom_init_trim(my_mutator_t *data, uint8_t *buf, size_t buf_size);
size_t  afl_custom_trim(my_mutator_t *data, uint8_t **out_buf);
int32_t afl_custom_post_trim(my_mutator_t *data, int success);

uint32_t afl_custom_fuzz_count(my_mutator_t *data, const uint8_t *buf,
                               size_t buf_size);
size_t   afl_custom_fuzz(my_mutator_t *data, uint8_t *buf, size_t buf_size,
                         uint8_t **out_buf, uint8_t *add_buf,
                         size_t add_buf_size,  // add_buf can be NULL
                         size_t max_size);
void     afl_custom_queue_new_entry(my_mutator_t * data,
                                    const uint8_t *filename_new_queue,
                                    const uint8_t *filename_orig_queue);

#ifdef __cplusplus
}
#endif

#endif
