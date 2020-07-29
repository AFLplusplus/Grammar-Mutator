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
  string                 tree_out_dir = "/tmp/afl_test_fuzz_out";

  CustomMutatorTest() {
    // Check whether the directory exists
    struct stat info;
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
  uint8_t *buf = nullptr;
  size_t   buf_size = 0;
  uint8_t  ret = 0;
  int32_t  stage_cur = 0;
  int32_t  stage_max = 0;

  // prepare a tree: 8 non-terminal nodes, 1 recursion edge
  auto tree = tree_create();
  auto start = node_create(START);
  tree->root = start;

  // start -> json
  node_init_subnodes(start, 1);
  auto json = node_create(JSON);
  node_set_subnode(start, 0, json);

  // json -> element
  node_init_subnodes(json, 1);
  auto element = node_create(ELEMENT);
  node_set_subnode(json, 0, element);

  // element -> ws_1, value ("true"), ws_2 (NULL)
  node_init_subnodes(element, 3);
  auto ws_1 = node_create(WS);
  auto value = node_create_with_val(VALUE, "true", 4);
  auto ws_2 = node_create(WS);
  node_set_subnode(element, 0, ws_1);
  node_set_subnode(element, 1, value);
  node_set_subnode(element, 2, ws_2);

  // ws_1 -> sp1_1 (" "), ws_3 (NULL)  (recursive)
  node_init_subnodes(ws_1, 2);
  auto sp1_1 = node_create_with_val(SP1, " ", 1);
  auto ws_3 = node_create(WS);
  node_set_subnode(ws_1, 0, sp1_1);
  node_set_subnode(ws_1, 1, ws_3);

  tree_serialize(tree);
  write_tree_to_file("/tmp/afl_test_fuzz_out/trees/trimming_0", tree->ser_buf,
                     tree->ser_len, 0);

  ret = afl_custom_queue_get(
      mutator->data,
      (const uint8_t *)"/tmp/afl_test_fuzz_out/queue/trimming_0");
  ASSERT_EQ(ret, 1);

  // always fail in trimming
  stage_cur = 0;
  stage_max =
      afl_custom_init_trim(mutator->data, tree->data_buf, tree->data_len);
  EXPECT_EQ(stage_max, 9);  // 8 non-terminal nodes, 1 recursion edge
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
  EXPECT_EQ(stage_max, 9);  // 8 non-terminal nodes, 1 recursion edge
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
