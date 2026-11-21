/*
 * Check that afl_custom_queue_get of a .g4 build writes the tree files.
 * Usage: queue_get_check OUT_DIR
 * OUT_DIR is a new folder, as the output folder of AFL++. The program writes
 * test cases to OUT_DIR/queue and calls afl_custom_queue_get for each. Then it
 * checks the tree files in OUT_DIR/trees. The grammar must accept "[1, [2]]"
 * and "[3]", and reject "{". The JSON and the JavaScript grammars do.
 * The exit code is 0 if all checks pass, or 1 if not.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "custom_mutator.h"

static int failures = 0;

static void fail(const char *what, const char *name) {

  fprintf(stderr, "[-] %s: %s\n", name, what);
  ++failures;

}

static void write_text(const char *path, const char *text) {

  FILE *f = fopen(path, "wb");
  if (!f || fwrite(text, 1, strlen(text), f) != strlen(text) || fclose(f)) {

    perror(path);
    exit(1);

  }

}

/* Check that `tree_path` holds a tree that gives the bytes of `text` */
static void check_tree_file(const char *tree_path, const char *text,
                            const char *name) {

  tree_t *tree = read_tree_from_file(tree_path);
  if (!tree) {

    fail("no tree file, or the tree file is not a tree", name);
    return;

  }

  if (!tree_to_buf(tree))
    fail("out of memory", name);
  else if (tree->data_len != strlen(text) ||
           memcmp(tree->data_buf, text, tree->data_len))
    fail("the tree does not give the test case bytes", name);
  tree_free(tree);

}

/* Give one test case to afl_custom_queue_get. `tree_name` is the expected
   name of the tree file, or NULL if the parser must reject the test case. */
static void check_entry(my_mutator_t *data, const char *out_dir,
                        const char *queue_name, const char *text,
                        const char *tree_name) {

  char queue_path[PATH_MAX], tree_path[PATH_MAX];
  snprintf(queue_path, sizeof(queue_path), "%s/queue/%s", out_dir, queue_name);
  write_text(queue_path, text);

  if (afl_custom_queue_get(data, (const uint8_t *)queue_path) != 1)
    fail("afl_custom_queue_get did not return 1", queue_name);

  if (tree_name) {

    if (!data->tree_cur) fail("the mutator has no tree", queue_name);
    snprintf(tree_path, sizeof(tree_path), "%s/trees/%s", out_dir, tree_name);
    check_tree_file(tree_path, text, queue_name);

  } else {

    if (data->tree_cur) fail("the mutator has a tree", queue_name);

  }

}

int main(int argc, char *argv[]) {

  if (argc != 2) {

    fprintf(stderr, "Usage: %s OUT_DIR\n", argv[0]);
    return 2;

  }

  const char *out_dir = argv[1];
  char        path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/queue", out_dir);
  if (mkdir(out_dir, 0777) || mkdir(path, 0777)) {

    perror(path);
    return 2;

  }

  my_mutator_t *data = afl_custom_init(NULL, 0);
  if (!data) {

    fprintf(stderr, "[-] afl_custom_init failed\n");
    return 1;

  }

  // A test case from the seeds: the tree name comes after ",orig:"
  check_entry(data, out_dir, "id:000000,time:0,execs:0,orig:a.json", "[1, [2]]",
              "a.json");
  // A new test case: the tree name is the queue name
  check_entry(data, out_dir, "id:000001,src:000000,op:havoc", "[3]",
              "id:000001,src:000000,op:havoc");
  // The parser rejects the test case: no tree file
  check_entry(data, out_dir, "id:000002,time:0,execs:0,orig:bad.json", "{",
              NULL);
  snprintf(path, sizeof(path), "%s/trees/bad.json", out_dir);
  if (access(path, F_OK) == 0)
    fail("a rejected test case has a tree file", "bad.json");

  // The second call reads the tree file of the first call. Change the test
  // case first: the tree must come from the tree file, not from a new parse.
  snprintf(path, sizeof(path), "%s/queue/id:000000,time:0,execs:0,orig:a.json",
           out_dir);
  write_text(path, "[4]");
  if (afl_custom_queue_get(data, (const uint8_t *)path) != 1 || !data->tree_cur)
    fail("no tree on the second call", "a.json");
  else if (!tree_to_buf(data->tree_cur) ||
           data->tree_cur->data_len != strlen("[1, [2]]") ||
           memcmp(data->tree_cur->data_buf, "[1, [2]]",
                  data->tree_cur->data_len))
    fail("the second call did not read the tree file", "a.json");

  afl_custom_deinit(data);
  if (failures) {

    fprintf(stderr, "[-] %d checks failed\n", failures);
    return 1;

  }

  printf("[+] afl_custom_queue_get wrote and read the tree files\n");
  return 0;

}
