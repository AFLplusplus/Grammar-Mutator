/*
 * Round-trip check for grammar_parser-<name>.
 * For each tree file in TREES_DIR, find the seed with the same base name under
 * SEEDS_DIR. Load the tree, convert it to bytes, and compare with the seed.
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

/* Return a malloc'ed path of the first file called `name` under `dir`. */
static char *find_seed(const char *dir, const char *name) {

  DIR *d = opendir(dir);
  if (!d) return NULL;

  char          *found = NULL;
  struct dirent *entry;
  while (!found && (entry = readdir(d)) != NULL) {

    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

    struct stat st;
    if (stat(path, &st) != 0) continue;

    if (S_ISDIR(st.st_mode))
      found = find_seed(path, name);
    else if (strcmp(entry->d_name, name) == 0)
      found = strdup(path);

  }

  closedir(d);
  return found;

}

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
    buf = realloc(buf, cap);

  }

  fclose(f);
  return buf;

}

int main(int argc, char **argv) {

  if (argc != 3) {

    fprintf(stderr, "Usage: %s SEEDS_DIR TREES_DIR\n", argv[0]);
    return 2;

  }

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

    char *seed_path = find_seed(argv[1], name);
    if (!seed_path) {

      printf("[-] %s: no seed\n", name);
      ++failed;
      continue;

    }

    size_t         seed_len = 0;
    unsigned char *seed = read_file(seed_path, &seed_len);
    tree_t        *tree = read_tree_from_file(tree_path);
    if (!seed || !tree) {

      printf("[-] %s: cannot load\n", name);
      ++failed;

    } else {

      tree_get_size(tree);
      tree_to_buf(tree);
      if (tree->data_len == seed_len &&
          (seed_len == 0 || memcmp(tree->data_buf, seed, seed_len) == 0)) {

        ++ok;

      } else {

        printf("[-] %s: bytes differ\n", name);
        ++failed;

      }

    }

    free(seed);
    tree_free(tree);
    free(seed_path);

  }

  closedir(d);

  if (failed || !ok) {

    if (!ok && !failed) printf("[-] No tree in %s\n", argv[2]);
    return 1;

  }

  printf("[+] %d trees match\n", ok);
  return 0;

}
