/*
 * A library that makes each readdir call fail with EIO. test_grammar_parser.sh
 * loads it into grammar_parser-<name> to test the readdir error path: with
 * LD_PRELOAD on Linux, and with DYLD_INSERT_LIBRARIES on macOS.
 */
#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <stddef.h>

#ifdef __APPLE__

static struct dirent *failing_readdir(DIR *dir) {

  (void)dir;
  errno = EIO;
  return NULL;

}

/* The dynamic loader of macOS calls failing_readdir in place of readdir */
__attribute__((used)) static const struct {

  const void *replacement;
  const void *original;

} interposers[] __attribute__((section("__DATA,__interpose"))) = {

    {(const void *)failing_readdir, (const void *)readdir}};

#else

struct dirent *readdir(DIR *dir) {

  (void)dir;
  errno = EIO;
  return NULL;

}

  #ifdef __GLIBC__
/* A build with 64-bit file offsets calls readdir64 */
struct dirent64 *readdir64(DIR *dir) {

  (void)dir;
  errno = EIO;
  return NULL;

}

  #endif

#endif
