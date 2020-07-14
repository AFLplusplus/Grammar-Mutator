#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>

#include <string>

#include "gtest/gtest.h"

typedef void *(*afl_custom_init_t)(void *afl, unsigned int seed);
typedef size_t (*afl_custom_fuzz_t)(void *data, uint8_t *buf, size_t buf_size,
                                    uint8_t **out_buf, uint8_t *add_buf,
                                    size_t add_buf_size, size_t max_size);
typedef void (*afl_custom_deinit_t)(void *data);
typedef uint8_t (*afl_custom_queue_get_t)(void *data, const uint8_t *filename);
typedef void (*afl_custom_queue_new_entry_t)(
    void *data, const uint8_t *filename_new_queue,
    const uint8_t *filename_orig_queue);

struct custom_mutator {
  void *                       data;
  afl_custom_init_t            afl_custom_init;
  afl_custom_fuzz_t            afl_custom_fuzz;
  afl_custom_deinit_t          afl_custom_deinit;
  afl_custom_queue_get_t       afl_custom_queue_get;
  afl_custom_queue_new_entry_t afl_custom_queue_new_entry;
};

static void dump_test_case(uint8_t *buf, size_t buf_size) {
  fprintf(stderr, "%.*s\n", (int)buf_size, buf);
}

static char *fn = nullptr;
static int   num = 100;

class CustomMutatorTest : public ::testing::Test {
 protected:
  void *                 dh = nullptr;
  void *                 afl = nullptr;
  struct custom_mutator *mutator = nullptr;

  CustomMutatorTest() = default;

  ~CustomMutatorTest() override = default;

  void SetUp() override {
    // Load the custom mutator library
    dh = dlopen(fn, RTLD_NOW);
    ASSERT_TRUE(dh != nullptr);
    printf("Load the custom mutator: %s\n", fn);

    mutator = (struct custom_mutator *)calloc(1, sizeof(struct custom_mutator));
    mutator->afl_custom_init = (afl_custom_init_t)dlsym(dh, "afl_custom_init");
    ASSERT_TRUE(mutator->afl_custom_init != nullptr);

    mutator->afl_custom_fuzz = (afl_custom_fuzz_t)dlsym(dh, "afl_custom_fuzz");
    ASSERT_TRUE(mutator->afl_custom_fuzz != nullptr);

    mutator->afl_custom_deinit =
        (afl_custom_deinit_t)dlsym(dh, "afl_custom_deinit");
    ASSERT_TRUE(mutator->afl_custom_deinit != nullptr);

    mutator->afl_custom_queue_get =
        (afl_custom_queue_get_t)dlsym(dh, "afl_custom_queue_get");
    ASSERT_TRUE(mutator->afl_custom_queue_get != nullptr);

    mutator->afl_custom_queue_new_entry =
        (afl_custom_queue_new_entry_t)dlsym(dh, "afl_custom_queue_new_entry");
    ASSERT_TRUE(mutator->afl_custom_queue_new_entry != nullptr);

    // Initialize the custom mutator
    mutator->data = mutator->afl_custom_init(afl, 0);  // fixed random seed
    ASSERT_TRUE(mutator->data != nullptr);
  }

  void TearDown() override {
    // Deinitialize the custom mutator
    mutator->afl_custom_deinit(mutator->data);
    mutator->data = nullptr;
    mutator->afl_custom_init = nullptr;
    mutator->afl_custom_fuzz = nullptr;
    mutator->afl_custom_deinit = nullptr;
    mutator->afl_custom_queue_get = nullptr;
    mutator->afl_custom_queue_new_entry = nullptr;
    dlclose(dh);
    dh = nullptr;
    free(mutator);
    mutator = nullptr;
  }
};

TEST_F(CustomMutatorTest, FuzzNTimes) {
  uint8_t *buf = nullptr;
  size_t   buf_size = 0;

  uint8_t ret =
      mutator->afl_custom_queue_get(mutator->data, (const uint8_t *)"seed");
  ASSERT_EQ(ret, 1);
  mutator->afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, 4096);
  ASSERT_TRUE(buf != nullptr);
  mutator->afl_custom_queue_new_entry(mutator->data, (const uint8_t *)"init",
                                      (const uint8_t *)"seed");

  ret = mutator->afl_custom_queue_get(mutator->data, (const uint8_t *)"init");
  ASSERT_EQ(ret, 1);
  for (int i = 0; i < num; ++i) {
    buf_size = mutator->afl_custom_fuzz(mutator->data, nullptr, 0, &buf,
                                        nullptr, 0, 4096);
    ASSERT_TRUE(buf != nullptr);
    std::string fn_new = std::to_string(i);
    mutator->afl_custom_queue_new_entry(mutator->data,
                                        (const uint8_t *)fn_new.c_str(),
                                        (const uint8_t *)"init");

    fprintf(stderr, "=====%d=====\n", i + 1);
    dump_test_case(buf, buf_size);
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc < 3) exit(EXIT_FAILURE);
  fn = argv[1];
  num = atoi(argv[2]);
  return RUN_ALL_TESTS();
}
