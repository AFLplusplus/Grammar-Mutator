#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include <string>

#include "custom_mutator.h"
#include "f1_c_fuzz.h"
#include "tree.h"

#include "gtest/gtest.h"

using namespace std;

struct custom_mutator {
  my_mutator_t *data;
};

static void dump_test_case(uint8_t *buf, size_t buf_size) {
  fprintf(stderr, "%.*s\n", (int)buf_size, buf);
}

static int num = 100;

class CustomMutatorTest : public ::testing::Test {
 protected:
  afl_t *                afl = nullptr;
  struct custom_mutator *mutator = nullptr;
  string                 out_dir = "/tmp/afl_test_fuzz_out";
  string                 tree_out_dir = out_dir + "/trees";
  string                 queue_dir = out_dir + "/queue";

  CustomMutatorTest() {
    // Check whether the directory exists
    struct stat info;
    if (stat(out_dir.c_str(), &info) != 0) {
      if (mkdir(out_dir.c_str(), 0700) != 0) {
        // error
        perror("Cannot create the directory (CustomMutatorTest)");
        exit(EXIT_FAILURE);
      }
    } else if (info.st_mode & S_IFDIR) {
      // directory exist
    } else {
      // Not a directory
      perror("Wrong output path (CustomMutatorTest)");
      exit(EXIT_FAILURE);
    }

    if (stat(tree_out_dir.c_str(), &info) != 0) {
      if (mkdir(tree_out_dir.c_str(), 0700) != 0) {
        // error
        perror("Cannot create the directory (CustomMutatorTest)");
        exit(EXIT_FAILURE);
      }
    } else if (info.st_mode & S_IFDIR) {
      // directory exist
    } else {
      // Not a directory
      perror("Wrong tree output path (CustomMutatorTest)");
      exit(EXIT_FAILURE);
    }

    if (stat(queue_dir.c_str(), &info) != 0) {
      if (mkdir(queue_dir.c_str(), 0700) != 0) {
        // error
        perror("Cannot create the directory (CustomMutatorTest)");
        exit(EXIT_FAILURE);
      }
    } else if (info.st_mode & S_IFDIR) {
      // directory exist
    } else {
      // Not a directory
      perror("Wrong queue output path (CustomMutatorTest)");
      exit(EXIT_FAILURE);
    }
  }

  ~CustomMutatorTest() override = default;

  void SetUp() override {
    mutator = (struct custom_mutator *)calloc(1, sizeof(struct custom_mutator));
    ASSERT_NE(mutator, nullptr);

    // Initialize the custom mutator
    mutator->data = afl_custom_init(afl, 0);  // fixed random seed
    ASSERT_NE(mutator->data, nullptr);
  }

  void TearDown() override {
    // Deinitialize the custom mutator
    afl_custom_deinit(mutator->data);
    mutator->data = nullptr;
    free(mutator);
    mutator = nullptr;
  }
};

TEST_F(CustomMutatorTest, FuzzNTimes) {
  uint8_t *buf = nullptr;
  size_t   buf_size = 0;

  // prepare a tree
  auto tree = gen_init__(100);
  dump_tree_to_test_case(tree, "/tmp/afl_test_fuzz_out/queue/fuzz_0");
  write_tree_to_file(tree, "/tmp/afl_test_fuzz_out/trees/fuzz_0");
  tree_free(tree);

  uint8_t ret = afl_custom_queue_get(
      mutator->data, (const uint8_t *)"/tmp/afl_test_fuzz_out/queue/fuzz_0");
  EXPECT_EQ(ret, 1);
  afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, 4096);
  EXPECT_NE(buf, nullptr);
  afl_custom_queue_new_entry(
      mutator->data, (const uint8_t *)"/tmp/afl_test_fuzz_out/queue/fuzz_1",
      (const uint8_t *)"/tmp/afl_test_fuzz_out/queue/fuzz_0");

  ret = afl_custom_queue_get(
      mutator->data, (const uint8_t *)"/tmp/afl_test_fuzz_out/queue/fuzz_1");
  EXPECT_EQ(ret, 1);
  for (int i = 0; i < num; ++i) {
    buf_size =
        afl_custom_fuzz(mutator->data, nullptr, 0, &buf, nullptr, 0, 4096);
    EXPECT_NE(buf, nullptr);
    string fn_new = "/tmp/afl_test_fuzz_out/queue/fuzz_1_" + to_string(i);
    afl_custom_queue_new_entry(
        mutator->data, (const uint8_t *)fn_new.c_str(),
        (const uint8_t *)"/tmp/afl_test_fuzz_out/queue/fuzz_1");

#ifdef DEBUG_BUILD
    fprintf(stderr, "=====%d=====\n", i + 1);
    dump_test_case(buf, buf_size);
#endif
  }
}

TEST_F(CustomMutatorTest, Trimming) {
  srandom(1234);

  uint8_t *buf = nullptr;
  size_t   buf_size = 0;
  uint8_t  ret = 0;
  int32_t  stage_cur = 0;
  int32_t  stage_max = 0;

  // prepare a tree
  auto tree = gen_init__(100);
  dump_tree_to_test_case(tree, "/tmp/afl_test_fuzz_out/queue/trimming_0");
  write_tree_to_file(tree, "/tmp/afl_test_fuzz_out/trees/trimming_0");

  ret = afl_custom_queue_get(
      mutator->data,
      (const uint8_t *)"/tmp/afl_test_fuzz_out/queue/trimming_0");
  ASSERT_EQ(ret, 1);

  // always fail in trimming
  stage_cur = 0;
  stage_max =
      afl_custom_init_trim(mutator->data, tree->data_buf, tree->data_len);
  //  EXPECT_EQ(stage_max, tree->root->non_term_size +
  //  tree->root->recursion_edge_size);
  while (stage_cur < stage_max) {
    buf_size = afl_custom_trim(mutator->data, &buf);
#ifdef DEBUG_BUILD
    dump_test_case(buf, buf_size);
#endif

    stage_cur = afl_custom_post_trim(mutator->data, 0);
  }

  // always success in trimming
  stage_cur = 0;
  stage_max =
      afl_custom_init_trim(mutator->data, tree->data_buf, tree->data_len);
  //  EXPECT_EQ(stage_max, tree->root->non_term_size +
  //  tree->root->recursion_edge_size);
  while (stage_cur < stage_max) {
    buf_size = afl_custom_trim(mutator->data, &buf);
#ifdef DEBUG_BUILD
    dump_test_case(buf, buf_size);
#endif

    stage_cur = afl_custom_post_trim(mutator->data, 1);
  }

  tree_free(tree);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc < 2) exit(EXIT_FAILURE);
  num = atoi(argv[1]);
  return RUN_ALL_TESTS();
}
