#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils.h"

bool create_directory(const char *path) {

  struct stat info;
  if (stat(path, &info) != 0) {

    if (mkdir(path, 0700) != 0) {

      // error, cannot create the directory
      return false;

    }

  } else if (info.st_mode & S_IFDIR) {

    // directory exist
    return true;

  } else {

    // not a directory, wrong path
    return false;

  }

  return true;

}

bool remove_directory(const char *path) {

  DIR *  d = opendir(path);
  size_t path_len = strlen(path);
  int    r = -1;

  if (d) {

    struct dirent *p;

    r = 0;
    while (!r && (p = readdir(d))) {

      int    r2 = -1;
      char * buf = NULL;
      size_t len;

      /* Skip the names "." and ".." as we don't want to recurse on them. */
      if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;

      len = path_len + strlen(p->d_name) + 2;
      buf = malloc(len);

      if (buf) {

        struct stat info;

        snprintf(buf, len, "%s/%s", path, p->d_name);
        if (!stat(buf, &info)) {

          if (S_ISDIR(info.st_mode))
            r2 = remove_directory(buf);
          else
            r2 = unlink(buf);

        }

        free(buf);

      }

      r = r2;

    }

    closedir(d);

  }

  if (!r) r = rmdir(path);

  if (!r) return false;

  return true;

}

