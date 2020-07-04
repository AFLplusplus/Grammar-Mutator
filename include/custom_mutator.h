#ifndef __CUSTOM_MUTATOR_H__
#define __CUSTOM_MUTATOR_H__

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#define _FILE_OFFSET_BITS 64

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "helpers.h"
#include "tree.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct afl {

} afl_t;

typedef struct my_mutator {

  afl_t *afl;

  const uint8_t *filename_cur;
  tree_t *tree_cur;

  tree_t *mutated_tree;

  // Reused buffers:
  BUF_VAR(uint8_t, fuzz);

} my_mutator_t;

my_mutator_t *afl_custom_init(afl_t *afl, unsigned int seed);
size_t afl_custom_fuzz(my_mutator_t *data, uint8_t *buf, size_t buf_size,
                       uint8_t **out_buf, uint8_t *add_buf,
                       size_t add_buf_size,  // add_buf can be NULL
                       size_t max_size);
uint8_t afl_custom_queue_get(my_mutator_t *data, const uint8_t *filename);
void afl_custom_queue_new_entry(my_mutator_t * data,
                                const uint8_t *filename_new_queue,
                                const uint8_t *filename_orig_queue);
void afl_custom_deinit(my_mutator_t *data);

#ifdef __cplusplus
}
#endif

#endif
