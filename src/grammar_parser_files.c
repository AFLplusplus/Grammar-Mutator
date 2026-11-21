/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/*
   grammar_parser-<name>: the messages, the folder walk, and the file I/O.
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "grammar_parser_internal.h"
#include "tree.h"

// Print a file name. Each byte that is not part of valid UTF-8 prints as
// "\xNN".
void gp_print_name(FILE *stream, const char *name) {

  const unsigned char *s = (const unsigned char *)name;

  while (*s) {

    size_t len = 0;
    if (s[0] < 0x80) {

      len = 1;

    } else if (s[0] >= 0xC2 && s[0] <= 0xDF) {

      if (s[1] >= 0x80 && s[1] <= 0xBF) len = 2;

    } else if (s[0] >= 0xE0 && s[0] <= 0xEF) {

      unsigned char lo = s[0] == 0xE0 ? 0xA0 : 0x80;
      unsigned char hi = s[0] == 0xED ? 0x9F : 0xBF;
      if (s[1] >= lo && s[1] <= hi && s[2] >= 0x80 && s[2] <= 0xBF) len = 3;

    } else if (s[0] >= 0xF0 && s[0] <= 0xF4) {

      unsigned char lo = s[0] == 0xF0 ? 0x90 : 0x80;
      unsigned char hi = s[0] == 0xF4 ? 0x8F : 0xBF;
      if (s[1] >= lo && s[1] <= hi && s[2] >= 0x80 && s[2] <= 0xBF &&
          s[3] >= 0x80 && s[3] <= 0xBF)
        len = 4;

    }

    if (len) {

      fwrite(s, 1, len, stream);
      s += len;

    } else {

      fprintf(stream, "\\x%02x", s[0]);
      ++s;

    }

  }

}

// Print one message line: `before`, the file name, `after`, and the text of
// `error` (an errno value) if it is not 0
void gp_report(const char *before, const char *name, const char *after,
               int error) {

  fputs(before, stderr);
  gp_print_name(stderr, name);
  fputs(after, stderr);
  if (error) fprintf(stderr, ": %s", strerror(error));
  fputc('\n', stderr);

}

void gp_out_of_memory(void) {

  fprintf(stderr, "[-] Out of memory\n");
  exit(2);

}

void *gp_xmalloc(size_t size) {

  void *p = malloc(size ? size : 1);
  if (!p) gp_out_of_memory();
  return p;

}

void *gp_xrealloc(void *p, size_t size) {

  p = realloc(p, size);
  if (!p) gp_out_of_memory();
  return p;

}

char *gp_xstrdup(const char *s) {

  char *p = strdup(s);
  if (!p) gp_out_of_memory();
  return p;

}

// Return "a/b". An empty `a` gives "b".
char *gp_join(const char *a, const char *b) {

  if (!*a) return gp_xstrdup(b);
  size_t a_len = strlen(a);
  while (a_len > 1 && a[a_len - 1] == '/')
    --a_len;
  size_t b_len = strlen(b);
  char  *s = gp_xmalloc(a_len + b_len + 2);
  memcpy(s, a, a_len);
  size_t pos = a_len;
  if (a[a_len - 1] != '/') s[pos++] = '/';
  memcpy(s + pos, b, b_len + 1);
  return s;

}

// Cut the absolute path `path` to its parent folder, as a name (no file
// system access). Return 0 if `path` is "/".
static int cut_to_parent(char *path) {

  size_t len = strlen(path);
  while (len > 1 && path[len - 1] == '/')
    --len;
  path[len] = '\0';
  char *slash = strrchr(path, '/');
  if (!slash || !strcmp(path, "/")) return 0;
  while (slash > path && slash[-1] == '/')
    --slash;
  if (slash == path)
    path[1] = '\0';
  else
    *slash = '\0';
  return 1;

}

// Compare relative paths component by component, in byte order
static int compare_jobs(const void *a, const void *b) {

  const unsigned char *x = (const unsigned char *)((const job_t *)a)->relative;
  const unsigned char *y = (const unsigned char *)((const job_t *)b)->relative;
  while (*x && *x == *y) {

    ++x;
    ++y;

  }

  // The end of a path comes first, then "/", then the other bytes
  int cx = *x == '\0' ? 0 : *x == '/' ? 1 : *x + 1;
  int cy = *y == '\0' ? 0 : *y == '/' ? 1 : *y + 1;
  return cx - cy;

}

// Find the regular files in `seeds_dir` and its subfolders. A symbolic link
// to a file counts as a file. The tool does not follow a symbolic link to a
// folder. Return 0, or -1 with a message.
int gp_find_files(const char *seeds_dir, job_t **jobs_out, size_t *count_out) {

  job_t *jobs = NULL;
  size_t count = 0, capacity = 0;
  char **folders = gp_xmalloc(sizeof(char *));  // relative paths ("" = top)
  size_t folder_count = 1, folder_capacity = 1;
  int    ret = 0;

  folders[0] = gp_xstrdup("");
  while (folder_count) {

    char *folder = folders[--folder_count];
    char *folder_path =
        *folder ? gp_join(seeds_dir, folder) : gp_xstrdup(seeds_dir);
    DIR *dir = opendir(folder_path);
    if (!dir) {

      gp_report("[-] Cannot read ", *folder ? folder : seeds_dir, "", errno);
      free(folder_path);
      free(folder);
      ret = -1;
      break;

    }

    // readdir gives NULL at the end and on an error. Only an error sets errno.
    struct dirent *entry;
    int            read_error = 0;
    for (;;) {

      errno = 0;
      entry = readdir(dir);
      if (!entry) {

        read_error = errno;
        break;

      }

      if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
      char       *relative = gp_join(folder, entry->d_name);
      char       *path = gp_join(seeds_dir, relative);
      struct stat st;
      int         is_file = 0, is_folder = 0;
      if (!lstat(path, &st)) {

        if (S_ISLNK(st.st_mode))
          is_file = !stat(path, &st) && S_ISREG(st.st_mode);
        else if (S_ISREG(st.st_mode))
          is_file = 1;
        else if (S_ISDIR(st.st_mode))
          is_folder = 1;

      }

      if (is_file) {

        if (count == capacity) {

          capacity = capacity ? 2 * capacity : 64;
          jobs = gp_xrealloc(jobs, capacity * sizeof(job_t));

        }

        memset(&jobs[count], 0, sizeof(job_t));
        jobs[count].path = path;
        jobs[count].relative = relative;
        ++count;

      } else if (is_folder) {

        if (folder_count == folder_capacity) {

          folder_capacity *= 2;
          folders = gp_xrealloc(folders, folder_capacity * sizeof(char *));

        }

        folders[folder_count++] = relative;
        free(path);

      } else {

        free(relative);
        free(path);

      }

    }

    closedir(dir);
    if (read_error)
      gp_report("[-] Cannot read the folder ", *folder ? folder : seeds_dir, "",
                read_error);
    free(folder_path);
    free(folder);
    if (read_error) {

      ret = -1;
      break;

    }

  }

  while (folder_count)
    free(folders[--folder_count]);
  free(folders);
  if (count) qsort(jobs, count, sizeof(job_t), compare_jobs);
  *jobs_out = jobs;
  *count_out = count;
  return ret;

}

// Read a file. Return its bytes, or NULL with errno set.
uint8_t *gp_read_file(const char *path, size_t *len_out) {

  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  size_t   capacity = 65536, len = 0;
  uint8_t *buf = malloc(capacity);
  while (buf) {

    len += fread(buf + len, 1, capacity - len, f);
    if (len < capacity) break;
    capacity *= 2;
    uint8_t *bigger = realloc(buf, capacity);
    if (!bigger) {

      free(buf);
      buf = NULL;

    } else {

      buf = bigger;

    }

  }

  int error = buf ? 0 : ENOMEM;
  if (buf && ferror(f)) error = errno ? errno : EIO;
  fclose(f);
  if (error) {

    free(buf);
    errno = error;
    return NULL;

  }

  *len_out = len;
  return buf;

}

// Remove the tree of an earlier run, which does not match the file now. A
// file that is not a tree stays. Return 0, or -1 with a message.
int gp_remove_old_tree(const char *tree_path, const char *relative) {

  struct stat st;
  if (stat(tree_path, &st) || !S_ISREG(st.st_mode)) return 0;

  size_t   len = 0;
  uint8_t *buf = gp_read_file(tree_path, &len);
  if (buf) {

    tree_t *tree = tree_deserialize(buf, len);
    free(buf);
    if (!tree) {

      gp_report("[!] ", tree_path, " is not a tree. It stays.", 0);
      return 0;

    }

    tree_free(tree);
    if (!unlink(tree_path)) return 0;

  }

  gp_report("[-] Cannot remove the old tree of ", relative, "", errno);
  return -1;

}

// Write a tree file. Return 0, or -1 with errno set.
int gp_write_file(const char *path, const uint8_t *buf, size_t len) {

  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  int ok = fwrite(buf, 1, len, f) == len;
  int error = errno;
  if (fclose(f)) {

    ok = 0;
    error = errno;

  }

  errno = error;
  return ok ? 0 : -1;

}

// Stop if TREES_DIR is SEEDS_DIR or a folder in it. The check compares the
// device and inode of the folders, not their names. So it also finds the same
// folder under a name with other case, a symbolic link, or a firmlink.
// Return 0, or -1 with a message.
int gp_check_folders(const char *seeds_dir, const char *trees_dir) {

  struct stat seeds, st;
  char       *folder = NULL, *real = NULL;
  int         same = 0, error = 0;

  if (stat(seeds_dir, &seeds)) {

    error = errno;

  } else if (trees_dir[0] == '/') {

    folder = gp_xstrdup(trees_dir);

  } else {

    char *cwd = getcwd(NULL, 0);
    if (!cwd) {

      error = errno;

    } else {

      folder = gp_join(cwd, trees_dir);
      free(cwd);

    }

  }

  // The folders that do not exist yet cannot be SEEDS_DIR. Start at the
  // deepest folder that exists, and walk up its real path.
  while (!error && stat(folder, &st)) {

    if (errno != ENOENT && errno != ENOTDIR)
      error = errno;
    else if (!cut_to_parent(folder))
      break;

  }

  if (!error && !(real = realpath(folder, NULL))) error = errno;
  while (!error && !same) {

    if (stat(real, &st))
      error = errno;
    else if (st.st_dev == seeds.st_dev && st.st_ino == seeds.st_ino)
      same = 1;
    else if (!cut_to_parent(real))
      break;

  }

  free(real);
  free(folder);
  if (error) {

    gp_report("[-] Cannot check TREES_DIR ", trees_dir, "", error);
    return -1;

  }

  if (same) {

    fprintf(stderr,
            "[-] TREES_DIR must not be SEEDS_DIR or a folder in SEEDS_DIR\n");
    return -1;

  }

  return 0;

}

// Make a folder and its parents (as "mkdir -p"). Return 0, or -1 with errno
// set. An empty path gives -1 with ENOENT.
int gp_make_folders(const char *path) {

  char *p = gp_xstrdup(path);
  for (char *c = p; *c; ++c) {

    // A "/" at the start is the root folder, not a parent to make
    if (*c != '/' || c == p) continue;
    *c = '\0';
    if (mkdir(p, 0777) && errno != EEXIST) {

      free(p);
      return -1;

    }

    *c = '/';

  }

  struct stat st;
  int         ret = 0;
  if (mkdir(p, 0777)) {

    if (errno != EEXIST) {

      ret = -1;

    } else if (stat(p, &st) || !S_ISDIR(st.st_mode)) {

      errno = EEXIST;
      ret = -1;

    }

  }

  free(p);
  return ret;

}
