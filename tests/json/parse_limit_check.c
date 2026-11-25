/*
 * Check that the parse time limit stops a slow parse of a JSON grammar build.
 * Usage: parse_limit_check OUT_DIR
 * The grammar is tests/json/ambiguous_list.json. It has two ways to read each
 * list, so the parse time of a long list grows exponentially. OUT_DIR is a new
 * folder. The program parses 40 short lists (the warm-up of the budget), two
 * long lists, and one more short list. The long lists must stop at the limit
 * and give no tree. The short lists must give a tree. The exit code is 0 if
 * all checks pass, or 1 if not.
 */
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "custom_mutator.h"

// The item count of a long list. Without the limit, its parse takes hours.
#define LONG_ITEMS 30

// The upper bound of a stopped parse, in seconds. The limit after the warm-up
// is about 50 ms. The bound leaves time for a slow machine and Valgrind.
#define LIMITED_MAX_S 2.5

static int failures = 0;

static void fail(const char *what, const char *name) {

  fprintf(stderr, "[-] %s: %s\n", name, what);
  ++failures;

}

// A parse without the limit does not end. Stop the check with a message.
static void on_alarm(int sig) {

  (void)sig;
  static const char msg[] = "[-] timeout: a parse did not stop\n";
  if (write(2, msg, sizeof(msg) - 1) < 0) _exit(1);
  _exit(1);

}

static void write_text(const char *path, const char *text) {

  FILE *f = fopen(path, "wb");
  if (!f || fwrite(text, 1, strlen(text), f) != strlen(text) || fclose(f)) {

    perror(path);
    exit(1);

  }

}

static double now_s(void) {

  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;

}

// Write "text" to the queue entry "name", and get the entry. Return the time
// of afl_custom_queue_get. Set "has_tree" if the entry has a tree.
static double get_entry(my_mutator_t *data, const char *queue, const char *name,
                        const char *text, int *has_tree) {

  char path[4096];
  snprintf(path, sizeof(path), "%s/%s", queue, name);
  write_text(path, text);
  double start = now_s();
  if (afl_custom_queue_get(data, (const uint8_t *)path) != 1)
    fail("afl_custom_queue_get did not return 1", name);
  double elapsed = now_s() - start;
  *has_tree = afl_custom_fuzz_count(data, NULL, 0) > 0;
  return elapsed;

}

int main(int argc, char **argv) {

  if (argc != 2) {

    fprintf(stderr, "Usage: %s OUT_DIR\n", argv[0]);
    return 2;

  }

  signal(SIGALRM, on_alarm);
  alarm(120);

  char queue[4096];
  snprintf(queue, sizeof(queue), "%s/queue", argv[1]);
  if (mkdir(argv[1], 0700) || mkdir(queue, 0700)) {

    perror(queue);
    return 1;

  }

  my_mutator_t *data = afl_custom_init(NULL, 0);
  if (!data) {

    fprintf(stderr, "[-] afl_custom_init failed\n");
    return 1;

  }

  int  has_tree;
  char name[64];
  for (int i = 0; i < 40; ++i) {

    snprintf(name, sizeof(name), "warm_%d", i);
    get_entry(data, queue, name, "[0,1]", &has_tree);
    if (!has_tree) fail("a short list has no tree", name);

  }

  char long_list[2 * LONG_ITEMS + 2] = "[0";
  for (int i = 1; i < LONG_ITEMS; ++i)
    strcat(long_list, ",0");
  strcat(long_list, "]");
  const char *long_names[] = {"long_1", "long_2"};
  for (int i = 0; i < 2; ++i) {

    double elapsed =
        get_entry(data, queue, long_names[i], long_list, &has_tree);
    if (has_tree) fail("the long list has a tree", long_names[i]);
    if (elapsed >= LIMITED_MAX_S) fail("the limit came too late", long_names[i]);
    printf("    %s: limited %.3f s\n", long_names[i], elapsed);

  }

  get_entry(data, queue, "short", "[0,1,0]", &has_tree);
  if (!has_tree) fail("a short list after the long lists has no tree", "short");

  afl_custom_deinit(data);
  if (failures) return 1;
  printf("[+] the time limit stopped the long parses\n");
  return 0;

}
