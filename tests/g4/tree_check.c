/*
 * Round-trip check for grammar_parser-<name>.
 * For each tree file in TREES_DIR, load the tree and convert it to bytes. The
 * bytes must equal a seed with the same base name under SEEDS_DIR.
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

/* Look for files called `name` under `dir`. Count them in `*found`. Return 1
 * if one of them has the bytes `buf`. The parser skips a file with a name that
 * it already used, so the tree can belong to any file with this name. */
static int match_seed(const char *dir, const char *name,
                      const unsigned char *buf, size_t len, int *found) {

  DIR *d = opendir(dir);
  if (!d) return 0;

  int            matched = 0;
  struct dirent *entry;
  while (!matched && (entry = readdir(d)) != NULL) {

    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

    struct stat st;
    if (stat(path, &st) != 0) continue;

    if (S_ISDIR(st.st_mode)) {

      matched = match_seed(path, name, buf, len, found);

    } else if (strcmp(entry->d_name, name) == 0) {

      ++*found;
      size_t         seed_len = 0;
      unsigned char *seed = read_file(path, &seed_len);
      matched = seed && seed_len == len &&
                (len == 0 || memcmp(seed, buf, len) == 0);
      free(seed);

    }

  }

  closedir(d);
  return matched;

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

    tree_t *tree = read_tree_from_file(tree_path);
    if (!tree) {

      printf("[-] %s: cannot load\n", name);
      ++failed;
      continue;

    }

    tree_get_size(tree);
    tree_to_buf(tree);
    int found = 0;
    if (match_seed(argv[1], name, tree->data_buf, tree->data_len, &found)) {

      ++ok;

    } else {

      printf("[-] %s: %s\n", name, found ? "bytes differ" : "no seed");
      ++failed;

    }

    tree_free(tree);

  }

  closedir(d);

  if (failed || !ok) {

    if (!ok && !failed) printf("[-] No tree in %s\n", argv[2]);
    return 1;

  }

  printf("[+] %d trees match\n", ok);
  return 0;

}
