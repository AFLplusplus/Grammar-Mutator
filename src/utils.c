#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "helpers.h"
#include "utils.h"

#define XXH_INLINE_ALL
#include "xxhash.h"
#undef XXH_INLINE_ALL

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


char * strrstr(const char * haystack, const char * needle) {

  size_t haystack_len;
  size_t needle_len;
  const char * s;

  // Don't allow empty needle or haystack
  if (!haystack || !needle) return NULL;

  haystack_len = strlen(haystack);
  needle_len = strlen(needle);

  // Don't allow empty needle or haystack or haystack smaller than needle
  if (!needle_len || !haystack_len || haystack_len < needle_len) return NULL;

  for (s = haystack + (haystack_len - needle_len); s != haystack; --s) {

    if (!strncmp(s, needle, needle_len)) return (char *) s;

  }

  return NULL;

}

// Random number generators
static RANDOM_RETURN random_seed[3];
#define ROTL(d, lrot) ((d << (lrot)) | (d >> (8 * sizeof(d) - (lrot))))
#define HASH_SEED 0xa5b35705

void random_set_seed(uint64_t seed) {

  random_seed[0] = XXH64(&seed, sizeof(seed), HASH_SEED);
  random_seed[1] = random_seed[0] ^ 0x1234567890abcdef;
  random_seed[2] = (random_seed[0] & 0x1234567890abcdef) ^
                   (random_seed[1] | 0xfedcba9876543210);

}

#ifdef WORD_SIZE_64
// romuDuoJr
//
// The fastest generator using 64-bit arith., but not suited for huge jobs.
// Est. capacity = 2^51 bytes. Register pressure = 4. State size = 128 bits.
RANDOM_RETURN random_next() {

  RANDOM_RETURN xp = random_seed[0];
  random_seed[0] = 15241094284759029579u * random_seed[1];
  random_seed[1] = random_seed[1] - xp;
  random_seed[1] = ROTL(random_seed[1], 27);
  return xp;

}
#else
// RomuTrio32
//
// 32-bit arithmetic: Good for general purpose use, except for huge jobs.
// Est. capacity >= 2^53 bytes. Register pressure = 5. State size = 96 bits.
RANDOM_RETURN random_next() {

  RANDOM_RETURN xp = random_seed[0], yp = random_seed[1], zp = random_seed[2];
  random_seed[0] = 3323815723u * zp;
  random_seed[1] = yp - xp;
  random_seed[1] = ROTL(random_seed[1], 6);
  random_seed[2] = zp - yp;
  random_seed[2] = ROTL(random_seed[2], 22);
  return xp;

}
#endif /* WORD_SIZE_64 */

uint32_t random_below(uint32_t limit) {

  if (limit <= 1) return 0;

  /* Modulo is biased - we don't want our fuzzing to be biased so let's do it
 right. See:
 https://stackoverflow.com/questions/10984974/why-do-people-say-there-is-modulo-bias-when-using-a-random-number-generator
 */
  uint64_t unbiased_rnd;
  do {

    unbiased_rnd = random_next();

  } while (unlikely(unbiased_rnd >= (UINT64_MAX - (UINT64_MAX % limit))));

  return unbiased_rnd % limit;

}
