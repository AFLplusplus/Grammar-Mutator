#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include <afl-fuzz.h>

static void usage(const char *name) {
  printf(
    "\n%s /path/to/custom/mutator/library\n\n", name);
}

static void dump_test_case(u8 *buf, size_t buf_size) {
  printf("%.*s", (int) buf_size, buf);
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
  u8 *buf = NULL;
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

  exit(EXIT_SUCCESS);
}
