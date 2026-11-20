/*
 * Round-trip check for grammar_parser-<name>.
 * For each tree file in TREES_DIR, load the tree and convert it to bytes. The
 * bytes must equal a seed under SEEDS_DIR with the same tree name: the base
 * name with each space changed to "_", as grammar_parser names the trees.
 * Usage: tree_check SEEDS_DIR TREES_DIR
 */
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "tree.h"

/* A seed file: its tree name and its path */
typedef struct {

  char *name;
  char *path;

} seed_t;

typedef struct {

  seed_t *items;
  size_t  len;
  size_t  cap;

} seed_list_t;

/* Read a whole file. Return NULL on error. */
static unsigned char *read_file(const char *path, size_t *len) {

  FILE *f = fopen(path, "rb");
  if (!f) return NULL;

  size_t         cap = 4096;
  unsigned char *buf = malloc(cap);
  *len = 0;
  while (buf) {

    size_t got = fread(buf + *len, 1, cap - *len, f);
    *len += got;
    if (*len < cap) break;
    cap *= 2;
    unsigned char *bigger = realloc(buf, cap);
    if (!bigger) free(buf);
    buf = bigger;

  }

  if (buf && ferror(f)) {

    free(buf);
    buf = NULL;

  }

  fclose(f);
  return buf;

}

static char *copy_string(const char *s) {

  char *copy = malloc(strlen(s) + 1);
  if (copy) strcpy(copy, s);
  return copy;

}

/* Add the files under `dir` to `seeds`. Return 0 on error. */
static int collect_seeds(const char *dir, seed_list_t *seeds) {

  DIR *d = opendir(dir);
  if (!d) {

    fprintf(stderr, "[-] Cannot open %s\n", dir);
    return 0;

  }

  int            ok = 1;
  struct dirent *entry;
  while (ok && (entry = readdir(d)) != NULL) {

    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

    struct stat st;
    if (stat(path, &st) != 0) continue;

    if (S_ISDIR(st.st_mode)) {

      ok = collect_seeds(path, seeds);
      continue;

    }

    if (seeds->len == seeds->cap) {

      size_t  cap = seeds->cap ? seeds->cap * 2 : 256;
      seed_t *items = realloc(seeds->items, cap * sizeof(seed_t));
      if (!items) {

        ok = 0;
        break;

      }

      seeds->items = items;
      seeds->cap = cap;

    }

    seed_t *seed = &seeds->items[seeds->len];
    seed->name = copy_string(entry->d_name);
    seed->path = copy_string(path);
    if (!seed->name || !seed->path) {

      free(seed->name);
      free(seed->path);
      ok = 0;
      break;

    }

    for (char *c = seed->name; *c; ++c)
      if (*c == ' ') *c = '_';
    ++seeds->len;

  }

  closedir(d);
  return ok;

}

static int compare_seeds(const void *a, const void *b) {

  return strcmp(((const seed_t *)a)->name, ((const seed_t *)b)->name);

}

/* Compare the bytes `buf` with the seeds called `name`. The parser skips a
 * file with a name that it already used, so the tree can belong to any of
 * them. Return NULL on a match, or else the reason. */
static const char *match_seed(const seed_list_t *seeds, const char *name,
                              const unsigned char *buf, size_t len) {

  // The first seed with the name (binary search in the sorted list)
  size_t lo = 0, hi = seeds->len;
  while (lo < hi) {

    size_t mid = lo + (hi - lo) / 2;
    if (strcmp(seeds->items[mid].name, name) < 0)
      lo = mid + 1;
    else
      hi = mid;

  }

  const char *reason = "no seed";
  for (size_t i = lo; i < seeds->len && strcmp(seeds->items[i].name, name) == 0;
       ++i) {

    size_t         seed_len = 0;
    unsigned char *seed = read_file(seeds->items[i].path, &seed_len);
    if (!seed) {

      reason = "cannot read the seed";
      continue;

    }

    int same = seed_len == len && (len == 0 || memcmp(seed, buf, len) == 0);
    free(seed);
    if (same) return NULL;
    reason = "bytes differ";

  }

  return reason;

}

int main(int argc, char **argv) {

  if (argc != 3) {

    fprintf(stderr, "Usage: %s SEEDS_DIR TREES_DIR\n", argv[0]);
    return 2;

  }

  seed_list_t seeds = {NULL, 0, 0};
  if (!collect_seeds(argv[1], &seeds)) return 2;
  if (seeds.len) qsort(seeds.items, seeds.len, sizeof(seed_t), compare_seeds);

  DIR *d = opendir(argv[2]);
  if (!d) {

    fprintf(stderr, "[-] Cannot open %s\n", argv[2]);
    return 2;

  }

  int            ok = 0, failed = 0;
  struct dirent *entry;
  while ((entry = readdir(d)) != NULL) {

    const char *name = entry->d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

    char tree_path[PATH_MAX];
    snprintf(tree_path, sizeof(tree_path), "%s/%s", argv[2], name);

    tree_t *tree = read_tree_from_file(tree_path);
    if (!tree) {

      printf("[-] %s: cannot load\n", name);
      ++failed;
      continue;

    }

    tree_get_size(tree);
    tree_to_buf(tree);
    const char *reason = match_seed(&seeds, name, tree->data_buf,
                                    tree->data_len);
    if (reason) {

      printf("[-] %s: %s\n", name, reason);
      ++failed;

    } else {

      ++ok;

    }

    tree_free(tree);

  }

  closedir(d);
  for (size_t i = 0; i < seeds.len; ++i) {

    free(seeds.items[i].name);
    free(seeds.items[i].path);

  }

  free(seeds.items);

  if (failed || !ok) {

    if (!ok && !failed) printf("[-] No tree in %s\n", argv[2]);
    return 1;

  }

  printf("[+] %d trees match\n", ok);
  return 0;

}
