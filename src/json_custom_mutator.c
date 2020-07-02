#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "helpers.h"
#include "parsing_tree.h"
#include "custom_mutator.h"


// JSON generator - extracted from F1 fuzzer
int max_depth = 3;
int map(int v) {
    return random() % v;
}


/**
 * Initialize this custom mutator
 *
 * @param[in] afl a pointer to the internal state object. Can be ignored for
 * now.
 * @param[in] seed A seed for this mutator - the same seed should always mutate
 * in the same way.
 * @return Pointer to the data object this custom mutator instance should use.
 *         There may be multiple instances of this mutator in one afl-fuzz run!
 *         Return NULL on error.
 */
my_mutator_t *afl_custom_init(afl_t *afl, unsigned int seed) {

  srandom(seed);

  my_mutator_t *data = calloc(1, sizeof(my_mutator_t));
  if (!data) {

    perror("afl_custom_init alloc");
    return NULL;

  }

  data->afl = afl;

  return data;

}

/**
 * Perform custom mutations on a given input
 *
 * (Optional for now. Required in the future)
 *
 * @param[in] data pointer returned in afl_custom_init for this fuzz case
 * @param[in] buf Pointer to input data to be mutated
 * @param[in] buf_size Size of input data
 * @param[out] out_buf the buffer we will work on. we can reuse *buf. NULL on
 * error.
 * @param[in] add_buf Buffer containing the additional test case
 * @param[in] add_buf_size Size of the additional test case
 * @param[in] max_size Maximum size of the mutated output. The mutation must not
 *     produce data larger than max_size.
 * @return Size of the mutated output.
 */
size_t afl_custom_fuzz(my_mutator_t *data, uint8_t *buf, size_t buf_size,
                       uint8_t **out_buf, uint8_t *add_buf,
                       size_t add_buf_size,  // add_buf can be NULL
                       size_t max_size) {

  // Randomly generate a JSON string
  max_depth = random() % 15 + 1; // randomly pick a `max_depth` within [1, 15]
  parsing_tree_t *tree = gen_init__();
  tree_to_buf(tree);

  size_t mutated_size = tree->data_len <= max_size ? tree->data_len : max_size;

  // maybe_grow is optimized to be quick for reused buffers.
  uint8_t *mutated_out = maybe_grow(BUF_PARAMS(data, fuzz), mutated_size);
  if (!mutated_out) {

    *out_buf = NULL;
    perror("custom mutator allocation (maybe_grow)");
    return 0;            /* afl-fuzz will very likely error out after this. */

  }

  memcpy(mutated_out, tree->data_buf, mutated_size);
  tree_free(tree);

  *out_buf = mutated_out;
  return mutated_size;

}

/**
 * Determine whether the fuzzer should fuzz the queue entry or not.
 *
 * (Optional)
 *
 * @param[in] data pointer returned in afl_custom_init for this fuzz case
 * @param filename File name of the test case in the queue entry
 * @return Return True(1) if the fuzzer will fuzz the queue entry, and
 *     False(0) otherwise.
 */
uint8_t afl_custom_queue_get(my_mutator_t *data, const uint8_t *filename) {

  return 1;

}

/**
 * Allow for additional analysis (e.g. calling a different tool that does a
 * different kind of coverage and saves this for the custom mutator).
 *
 * (Optional)
 *
 * @param data pointer returned in afl_custom_init for this fuzz case
 * @param filename_new_queue File name of the new queue entry
 * @param filename_orig_queue File name of the original queue entry
 */
void afl_custom_queue_new_entry(my_mutator_t * data,
                                const uint8_t *filename_new_queue,
                                const uint8_t *filename_orig_queue) {

  /* Additional analysis on the original or new test case */

}

/**
 * Deinitialize everything
 *
 * @param data The data ptr from afl_custom_init
 */
void afl_custom_deinit(my_mutator_t *data) {

  free(data->fuzz_buf);
  free(data);

}
