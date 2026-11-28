/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Written by Shengtuo Hu

   Copyright 2020 AFLplusplus Project. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   A grammar-based custom mutator written for GSoC '20.

 */

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mutator_stats.h"

namespace fs = std::filesystem;
using std::string;
using std::vector;

static const char *kVar = "GRAMMAR_MUTATOR_STATS_SECONDS";
static const char *kHeader =
    "# unix_time, parses, failed_parses, limit_parses, parser_states, "
    "lexer_states, heap_kb";
static const uint64_t kSecond = 1000000000ull;

// The lines of the file "path", without the line ends
static vector<string> read_lines(const fs::path &path) {

  vector<string> lines;
  std::ifstream  in(path);
  string         line;
  while (std::getline(in, line))
    lines.push_back(line);
  return lines;

}

// The values of a statistics line
static vector<string> split_fields(const string &line) {

  vector<string> fields;
  size_t         start = 0;
  for (;;) {

    size_t end = line.find(", ", start);
    fields.push_back(line.substr(start, end - start));
    if (end == string::npos) break;
    start = end + 2;

  }

  return fields;

}

// The number of times "needle" occurs in "text"
static size_t count_of(const string &text, const string &needle) {

  size_t count = 0;
  for (size_t at = text.find(needle); at != string::npos;
       at = text.find(needle, at + needle.size()))
    ++count;
  return count;

}

// A temporary folder for each test
class MutatorStatsFile : public ::testing::Test {

 protected:
  fs::path dir;

  void SetUp() override {

    string tmpl = (fs::temp_directory_path() / "gm_stats_XXXXXX").string();
    ASSERT_NE(mkdtemp(tmpl.data()), nullptr);
    dir = tmpl;

  }

  void TearDown() override {

    std::error_code ec;
    fs::remove_all(dir, ec);
    unsetenv(kVar);

  }

  fs::path stats_file(const fs::path &folder) const {

    return folder / MUTATOR_STATS_FILE_NAME;

  }

};

TEST(MutatorStatsEnv, Values) {

  unsetenv(kVar);
  EXPECT_EQ(mutator_stats_seconds_from_env(), 0u);

  struct {

    const char *value;
    uint32_t    seconds;
    bool        warns;

  } cases[] = {

      {"", 0, false},          {"0", 0, false},
      {"1", 1, false},         {"60", 60, false},
      {"86400", 86400, false}, {"86401", 0, true},
      {"-5", 0, true},         {"60s", 0, true},
      {" 60", 0, true},        {"99999999999999999999999", 0, true},

  };

  for (const auto &c : cases) {

    setenv(kVar, c.value, 1);
    testing::internal::CaptureStderr();
    EXPECT_EQ(mutator_stats_seconds_from_env(), c.seconds) << c.value;
    string err = testing::internal::GetCapturedStderr();
    EXPECT_EQ(count_of(err, kVar), c.warns ? 1u : 0u) << c.value;

  }

  unsetenv(kVar);

}

TEST_F(MutatorStatsFile, FirstLineAtOpen) {

  mutator_stats_t *stats = mutator_stats_create(60);
  ASSERT_NE(stats, nullptr);
  mutator_stats_open(stats, dir.c_str(), kSecond);

  vector<string> lines = read_lines(stats_file(dir));
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(lines[0], kHeader);

  vector<string> fields = split_fields(lines[1]);
  ASSERT_EQ(fields.size(), 7u);
  EXPECT_GT(std::stoll(fields[0]), 0);
  EXPECT_EQ(fields[1], "0");
  EXPECT_EQ(fields[2], "0");
  EXPECT_EQ(fields[3], "0");
  EXPECT_GE(std::stoll(fields[4]), 0);
  EXPECT_GE(std::stoll(fields[5]), 0);

  mutator_stats_free(stats);

}

TEST_F(MutatorStatsFile, LineEveryInterval) {

  const uint64_t   t0 = kSecond;
  mutator_stats_t *stats = mutator_stats_create(60);
  ASSERT_NE(stats, nullptr);
  mutator_stats_open(stats, dir.c_str(), t0);

  mutator_stats_tick(stats, t0 + 60 * kSecond - 1);
  EXPECT_EQ(read_lines(stats_file(dir)).size(), 2u);
  mutator_stats_tick(stats, t0 + 60 * kSecond);
  EXPECT_EQ(read_lines(stats_file(dir)).size(), 3u);
  mutator_stats_tick(stats, t0 + 60 * kSecond + 1);
  EXPECT_EQ(read_lines(stats_file(dir)).size(), 3u);
  mutator_stats_tick(stats, t0 + 120 * kSecond);
  EXPECT_EQ(read_lines(stats_file(dir)).size(), 4u);

  mutator_stats_free(stats);

}

TEST_F(MutatorStatsFile, CountsInLines) {

  mutator_stats_t *stats = mutator_stats_create(60);
  ASSERT_NE(stats, nullptr);
  mutator_stats_open(stats, dir.c_str(), kSecond);

  mutator_stats_count_parse(stats, true, PARSE_OK);
  mutator_stats_count_parse(stats, false, PARSE_SYNTAX_ERROR);
  mutator_stats_count_parse(stats, false, PARSE_LIMIT);
  mutator_stats_tick(stats, 61 * kSecond);

  vector<string> lines = read_lines(stats_file(dir));
  ASSERT_EQ(lines.size(), 3u);
  vector<string> fields = split_fields(lines[2]);
  ASSERT_EQ(fields.size(), 7u);
  EXPECT_EQ(fields[1], "3");
  EXPECT_EQ(fields[2], "2");
  EXPECT_EQ(fields[3], "1");

  mutator_stats_free(stats);

}

TEST_F(MutatorStatsFile, LastLineAtFree) {

  mutator_stats_t *stats = mutator_stats_create(60);
  ASSERT_NE(stats, nullptr);
  mutator_stats_open(stats, dir.c_str(), kSecond);
  mutator_stats_free(stats);

  EXPECT_EQ(read_lines(stats_file(dir)).size(), 3u);

}

TEST_F(MutatorStatsFile, HeaderOnceOnAppend) {

  for (int run = 0; run < 2; ++run) {

    mutator_stats_t *stats = mutator_stats_create(60);
    ASSERT_NE(stats, nullptr);
    mutator_stats_open(stats, dir.c_str(), kSecond);
    mutator_stats_free(stats);

  }

  vector<string> lines = read_lines(stats_file(dir));
  ASSERT_EQ(lines.size(), 5u);
  EXPECT_EQ(lines[0], kHeader);
  for (size_t i = 1; i < lines.size(); ++i)
    EXPECT_NE(lines[i][0], '#') << i;

}

TEST_F(MutatorStatsFile, SecondOpenIsNoOp) {

  fs::path a = dir / "a", b = dir / "b";
  ASSERT_TRUE(fs::create_directory(a));
  ASSERT_TRUE(fs::create_directory(b));

  mutator_stats_t *stats = mutator_stats_create(60);
  ASSERT_NE(stats, nullptr);
  mutator_stats_open(stats, a.c_str(), kSecond);
  mutator_stats_open(stats, b.c_str(), kSecond);
  mutator_stats_free(stats);

  EXPECT_TRUE(fs::exists(stats_file(a)));
  EXPECT_FALSE(fs::exists(stats_file(b)));

}

TEST_F(MutatorStatsFile, OpenFailureKeepsOff) {

  // A folder with the name of the file makes the open fail
  ASSERT_TRUE(fs::create_directory(stats_file(dir)));

  mutator_stats_t *stats = mutator_stats_create(60);
  ASSERT_NE(stats, nullptr);
  testing::internal::CaptureStderr();
  mutator_stats_open(stats, dir.c_str(), kSecond);
  mutator_stats_open(stats, dir.c_str(), kSecond);
  mutator_stats_tick(stats, 61 * kSecond);
  mutator_stats_free(stats);
  string err = testing::internal::GetCapturedStderr();

  EXPECT_EQ(count_of(err, "cannot open the statistics file"), 1u);
  EXPECT_TRUE(fs::is_directory(stats_file(dir)));

}
