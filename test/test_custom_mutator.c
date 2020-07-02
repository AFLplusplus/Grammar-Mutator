#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "custom_mutator.h"

struct custom_mutator {

  const char *name;
  void *      dh;
  uint8_t *        post_process_buf;
  size_t      post_process_size;
  uint8_t          stacked_custom_prob, stacked_custom;

  void *data;
  void *(*afl_custom_init)(afl_t *afl, unsigned int seed);
  size_t (*afl_custom_fuzz)(void *data, uint8_t *buf, size_t buf_size, uint8_t **out_buf,
                            uint8_t *add_buf, size_t add_buf_size, size_t max_size);
  size_t (*afl_custom_post_process)(void *data, uint8_t *buf, size_t buf_size,
                                    uint8_t **out_buf);
  size_t (*afl_custom_init_trim)(void *data, uint8_t *buf, size_t buf_size);
  size_t (*afl_custom_trim)(void *data, uint8_t **out_buf);
  size_t (*afl_custom_post_trim)(void *data, uint8_t success);
  size_t (*afl_custom_havoc_mutation)(void *data, uint8_t *buf, size_t buf_size,
                                      uint8_t **out_buf, size_t max_size);
  uint8_t (*afl_custom_havoc_mutation_probability)(void *data);
  uint8_t (*afl_custom_queue_get)(void *data, const uint8_t *filename);
  void (*afl_custom_queue_new_entry)(void *data, const uint8_t *filename_new_queue,
                                     const uint8_t *filename_orig_queue);
  void (*afl_custom_deinit)(void *data);

};

static void usage(const char *name) {
  printf(
    "\n%s /path/to/custom/mutator/library\n\n", name);
}

static void dump_test_case(uint8_t *buf, size_t buf_size) {
  printf("%.*s\n", (int) buf_size, buf);
}

int main(int argc, char const *argv[]) {
  if (argc < 2) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  const char *fn = argv[1];
  void *dh = NULL;
  void *afl = NULL;
  struct custom_mutator *mutator = NULL;
  uint8_t *buf = NULL;
  size_t buf_size = 0;

  // Load the custom mutator library
  dh = dlopen(fn, RTLD_NOW);
  if (!dh) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }
  printf("Load the custom mutator: %s\n", argv[1]);

  mutator = calloc(1, sizeof(struct custom_mutator));
  mutator->afl_custom_init = dlsym(dh, "afl_custom_init");
  if (!mutator->afl_custom_init) {
    fprintf(stderr, "Symbol 'afl_custom_init' not found.\n");
    exit(EXIT_FAILURE);
  }

  mutator->afl_custom_fuzz = dlsym(dh, "afl_custom_fuzz");
  if (!mutator->afl_custom_fuzz) {
    fprintf(stderr, "Symbol 'afl_custom_fuzz' not found.\n");
    exit(EXIT_FAILURE);
  }

  mutator->afl_custom_deinit = dlsym(dh, "afl_custom_deinit");
  if (!mutator->afl_custom_deinit) {
    fprintf(stderr, "Symbol 'afl_custom_deinit' not found.\n");
    exit(EXIT_FAILURE);
  }

  // Initialize the custom mutator
  mutator->data = mutator->afl_custom_init(afl, 0); // fixed random seed

  for (int i = 0; i < 100; ++i) {
    buf_size = mutator->afl_custom_fuzz(
      mutator->data, NULL, 0, &buf, NULL, 0, 4096);
    printf("=====%d=====\n", i + 1);
    dump_test_case(buf, buf_size);
  }

  // Deinitialize the custom mutator
  mutator->afl_custom_deinit(mutator->data);

  dlclose(dh);
  free(mutator);

  exit(EXIT_SUCCESS);
}
