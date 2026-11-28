/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   A grammar-based custom mutator written for GSoC '20.

 */

#include "utils.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include "gtest/gtest.h"

TEST(UtilsTest, HashBytesIsStableAndNotZero) {

  EXPECT_EQ(hash_bytes("abc", 3), hash_bytes("abc", 3));
  EXPECT_NE(hash_bytes("abc", 3), hash_bytes("abd", 3));
  EXPECT_NE(hash_bytes("abc", 3), hash_bytes("abc", 2));
  EXPECT_NE(hash_bytes("", 0), 0u);

}

TEST(UtilsTest, HeapInUseGrows) {

  long before = heap_in_use_kb();
  if (before < 0) GTEST_SKIP() << "no malloc statistics (or Valgrind)";

  const size_t size = 16u << 20;
  char        *p = (char *)malloc(size);
  ASSERT_NE(p, nullptr);
  memset(p, 1, size);
  EXPECT_GE(heap_in_use_kb() - before, 15 * 1024);
  free(p);

}

TEST(UtilsTest, HashSetAddsEachHashOnce) {

  hash_set_t *set = hash_set_create();
  ASSERT_NE(set, nullptr);
  EXPECT_TRUE(hash_set_add(set, 1));
  EXPECT_FALSE(hash_set_add(set, 1));
  EXPECT_TRUE(hash_set_add(set, 2));
  EXPECT_TRUE(hash_set_add(set, 0));
  EXPECT_FALSE(hash_set_add(set, 0));
  EXPECT_EQ(hash_set_size(set), 3u);
  hash_set_free(set);

}

TEST(UtilsTest, HashSetGrows) {

  hash_set_t *set = hash_set_create();
  ASSERT_NE(set, nullptr);
  for (uint64_t i = 1; i <= 10000; ++i)
    EXPECT_TRUE(hash_set_add(set, i * 0x9e3779b97f4a7c15ULL));
  for (uint64_t i = 1; i <= 10000; ++i)
    EXPECT_FALSE(hash_set_add(set, i * 0x9e3779b97f4a7c15ULL));
  EXPECT_EQ(hash_set_size(set), 10000u);
  hash_set_free(set);

}

// Write a short file
static void touch_file(const char *fn) {

  FILE *f = fopen(fn, "wb");
  ASSERT_NE(f, nullptr);
  fputs("x", f);
  fclose(f);

}

static bool path_exists(const char *path) {

  struct stat info;
  return lstat(path, &info) == 0;

}

// Remove the folders of a test, also after a failed or stopped run. It does
// not follow a symbolic link.
static void remove_test_folders() {

  for (const char *name : {"rd_ok", "rd_missing", "rd_inside", "rd_outside"})
    std::filesystem::remove_all(name);

}

TEST(UtilsTest, RemoveDirectoryReturnsTrueOnSuccess) {

  remove_test_folders();
  ASSERT_EQ(mkdir("rd_ok", 0700), 0);
  ASSERT_EQ(mkdir("rd_ok/a", 0700), 0);
  ASSERT_EQ(mkdir("rd_ok/a/b", 0700), 0);
  touch_file("rd_ok/a/f");

  EXPECT_TRUE(remove_directory("rd_ok"));
  EXPECT_FALSE(path_exists("rd_ok"));
  remove_test_folders();

}

TEST(UtilsTest, RemoveDirectoryReturnsFalseForAMissingPath) {

  remove_test_folders();
  EXPECT_FALSE(remove_directory("rd_missing"));

}

// A symbolic link in the folder is removed. The function does not enter it, so
// the files of its target stay. A dangling link is removed too.
TEST(UtilsTest, RemoveDirectoryDoesNotFollowALink) {

  remove_test_folders();
  ASSERT_EQ(mkdir("rd_outside", 0700), 0);
  touch_file("rd_outside/keep");
  ASSERT_EQ(mkdir("rd_inside", 0700), 0);
  ASSERT_EQ(symlink("../rd_outside", "rd_inside/link"), 0);
  ASSERT_EQ(symlink("../rd_nothing", "rd_inside/dangling"), 0);

  EXPECT_TRUE(remove_directory("rd_inside"));
  EXPECT_FALSE(path_exists("rd_inside"));
  EXPECT_TRUE(path_exists("rd_outside/keep"));

  remove_test_folders();

}
