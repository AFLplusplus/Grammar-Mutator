#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <map>
#include <string>

#include "helpers.h"
#include "parsing_tree.h"
#include "custom_mutator.h"

using namespace std;

map<string, parsing_tree_t*> parsing_trees;

// JSON generator - extracted from F1 fuzzer
int max_depth = 3;
int map_rand(int v) {
    return random() % v;
}


my_mutator_t *afl_custom_init(afl_t *afl, unsigned int seed) {

  srandom(seed);

  my_mutator_t *data = (my_mutator_t *) calloc(1, sizeof(my_mutator_t));
  if (!data) {

    perror("afl_custom_init alloc");
    return NULL;

  }

  data->afl = afl;

  return data;

}

size_t afl_custom_fuzz(my_mutator_t *data, uint8_t *buf, size_t buf_size,
                       uint8_t **out_buf, uint8_t *add_buf,
                       size_t add_buf_size,  // add_buf can be NULL
                       size_t max_size) {

  printf("fuzz\n");

  // Randomly generate a JSON string
  max_depth = random() % 15 + 1; // randomly pick a `max_depth` within [1, 15]
  parsing_tree_t *tree = gen_init__();
  tree_to_buf(tree);

  size_t mutated_size = tree->data_len <= max_size ? tree->data_len : max_size;

  // maybe_grow is optimized to be quick for reused buffers.
  uint8_t *mutated_out = (uint8_t *) maybe_grow(
    BUF_PARAMS(data, fuzz), mutated_size);
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

uint8_t afl_custom_queue_get(my_mutator_t *data, const uint8_t *filename) {

  printf("get: %s\n", filename);
  return 1;

}

void afl_custom_queue_new_entry(my_mutator_t * data,
                                const uint8_t *filename_new_queue,
                                const uint8_t *filename_orig_queue) {

  printf("orig: %s\n", filename_orig_queue);
  printf("new: %s\n", filename_new_queue);

}

void afl_custom_deinit(my_mutator_t *data) {

  free(data->fuzz_buf);
  free(data);

}
