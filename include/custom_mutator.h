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

typedef struct afl {
} afl_t;

typedef struct my_mutator {
  afl_t *afl;

  const uint8_t *filename_cur;
  tree_t *       tree_cur;
  tree_t *       mutated_tree;
  tree_t *       trimmed_tree;

  // Trimming
  uint8_t cur_trimming_stage;  // 0: subtree trimming
                               // 1: recursive trimming

  size_t cur_subtree_trimming_step;
  size_t total_subtree_trimming_steps;

  size_t cur_recursive_trimming_step;
  size_t total_recursive_trimming_steps;

  // Reused buffers:
  BUF_VAR(uint8_t, fuzz);

  // Tree output directory
  char tree_out_dir[PATH_MAX];
  bool tree_out_dir_exist;

} my_mutator_t;

my_mutator_t *afl_custom_init(afl_t *afl, unsigned int seed);
void          afl_custom_deinit(my_mutator_t *data);

uint8_t afl_custom_queue_get(my_mutator_t *data, const uint8_t *filename);
int32_t afl_custom_init_trim(my_mutator_t *data, uint8_t *buf, size_t buf_size);
size_t  afl_custom_trim(my_mutator_t *data, uint8_t **out_buf);
int32_t afl_custom_post_trim(my_mutator_t *data, int success);
size_t  afl_custom_fuzz(my_mutator_t *data, uint8_t *buf, size_t buf_size,
                        uint8_t **out_buf, uint8_t *add_buf,
                        size_t add_buf_size,  // add_buf can be NULL
                        size_t max_size);
void    afl_custom_queue_new_entry(my_mutator_t * data,
                                   const uint8_t *filename_new_queue,
                                   const uint8_t *filename_orig_queue);

#ifdef __cplusplus
}
#endif

#endif
