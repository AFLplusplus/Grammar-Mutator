#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "custom_mutator.h"

#include "gtest/gtest.h"

typedef void *(*afl_custom_init_t)(void *afl, unsigned int seed);
typedef size_t (*afl_custom_fuzz_t)(
  void *data, uint8_t *buf, size_t buf_size, uint8_t **out_buf,
  uint8_t *add_buf, size_t add_buf_size, size_t max_size);
typedef void (*afl_custom_deinit_t)(void *data);

struct custom_mutator {
  void *data;
  afl_custom_init_t afl_custom_init;
  afl_custom_fuzz_t afl_custom_fuzz;
  afl_custom_deinit_t afl_custom_deinit;
};

static void dump_test_case(uint8_t *buf, size_t buf_size) {
  fprintf(stderr, "%.*s\n", (int) buf_size, buf);
}

char *fn = nullptr;

class CustomMutatorTest : public ::testing::Test {
protected:
  void *dh;
  void *afl = nullptr;
  struct custom_mutator *mutator = nullptr;

  CustomMutatorTest() {

  }

  ~CustomMutatorTest() override {

  }

  void SetUp() override {
    // Load the custom mutator library
    dh = dlopen(fn, RTLD_NOW);
    ASSERT_TRUE(dh != nullptr);
    printf("Load the custom mutator: %s\n", fn);

    mutator = (struct custom_mutator *) calloc(1, sizeof(struct custom_mutator));
    mutator->afl_custom_init = (afl_custom_init_t) dlsym(dh, "afl_custom_init");
    ASSERT_TRUE(mutator->afl_custom_init != nullptr);

    mutator->afl_custom_fuzz = (afl_custom_fuzz_t) dlsym(dh, "afl_custom_fuzz");
    ASSERT_TRUE(mutator->afl_custom_fuzz != nullptr);

    mutator->afl_custom_deinit = (afl_custom_deinit_t) dlsym(dh, "afl_custom_deinit");
    ASSERT_TRUE(mutator->afl_custom_deinit != nullptr);

    // Initialize the custom mutator
    mutator->data = mutator->afl_custom_init(afl, 0); // fixed random seed
    ASSERT_TRUE(mutator->data != nullptr);
  }

  void TearDown() override {
    // Deinitialize the custom mutator
    mutator->afl_custom_deinit(mutator->data);
    dlclose(dh);
    dh = nullptr;
    free(mutator);
    mutator = nullptr;
  }
};


TEST_F(CustomMutatorTest, Fuzz100Times) {
  uint8_t *buf = nullptr;
  size_t buf_size = 0;

  for (int i = 0; i < 100; ++i) {
    buf_size = mutator->afl_custom_fuzz(
      mutator->data, nullptr, 0, &buf, nullptr, 0, 4096);
    ASSERT_TRUE(buf != nullptr);

    fprintf(stderr, "=====%d=====\n", i + 1);
    dump_test_case(buf, buf_size);
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc < 2) {
    exit(EXIT_FAILURE);
  }
  fn = argv[1];
  return RUN_ALL_TESTS();
}
