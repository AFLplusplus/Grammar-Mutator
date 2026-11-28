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

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "custom_mutator.h"
#include "mutator_stats.h"
#include "utils.h"

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

TEST_F(MutatorStatsFile, FileIsCloseOnExec) {

  mutator_stats_t *stats = mutator_stats_create(60);
  ASSERT_NE(stats, nullptr);
  mutator_stats_open(stats, dir.c_str(), kSecond);

  // Find the descriptor of the file. The fuzzed target must not get it.
  struct stat want;
  ASSERT_EQ(stat(stats_file(dir).c_str(), &want), 0);
  int found = 0;
  for (int fd = 3; fd < 1024; ++fd) {

    struct stat got;
    if (fstat(fd, &got) || got.st_dev != want.st_dev ||
        got.st_ino != want.st_ino)
      continue;
    ++found;
    EXPECT_TRUE(fcntl(fd, F_GETFD) & FD_CLOEXEC) << fd;

  }

  EXPECT_EQ(found, 1);
  mutator_stats_free(stats);

}

// The mutator with the statistics file on or off. Each test starts the
// mutator in the output folder of one AFL++ instance.
class MutatorStatsMutator : public MutatorStatsFile {

 protected:
  my_mutator_t *data = nullptr;

  // Set or unset the variable, make "<instance>/queue", and start the mutator
  void start(const char *seconds, const fs::path &instance) {

    if (seconds)
      setenv(kVar, seconds, 1);
    else
      unsetenv(kVar);
    fs::create_directories(instance / "queue");
    data = afl_custom_init(nullptr, 0);  // fixed random seed
    ASSERT_NE(data, nullptr);

  }

  void stop() {

    if (data) afl_custom_deinit(data);
    data = nullptr;

  }

  void TearDown() override {

    stop();
    MutatorStatsFile::TearDown();

  }

  // Write the test case "name" with "bytes" into "folder"
  static string write_entry(const fs::path &folder, const char *name,
                            const string &bytes) {

    fs::path path = folder / name;
    std::ofstream(path, std::ios::binary) << bytes;
    return path.string();

  }

  uint8_t queue_get(const string &fn) {

    return afl_custom_queue_get(data, (const uint8_t *)fn.c_str());

  }

};

// A valid entry and an entry that is not UTF-8, so the parser rejects it
static const string kValid = "puts 1\n";
static const string kNotUtf8 = "\xff\xfe\n";

TEST_F(MutatorStatsMutator, OffByDefault) {

  fs::path instance = dir / "out" / "default";
  ASSERT_NO_FATAL_FAILURE(start(nullptr, instance));
  queue_get(write_entry(instance / "queue", "a", kValid));
  stop();

  EXPECT_FALSE(fs::exists(stats_file(instance)));

}

TEST_F(MutatorStatsMutator, NoFileOutsideQueue) {

  fs::path instance = dir / "out" / "default";
  ASSERT_NO_FATAL_FAILURE(start("3600", instance));
  fs::create_directories(dir / "seeds");
  queue_get(write_entry(dir / "seeds", "a", kValid));
  stop();

  for (const auto &entry : fs::recursive_directory_iterator(dir))
    EXPECT_NE(entry.path().filename(), MUTATOR_STATS_FILE_NAME) << entry.path();

}

TEST_F(MutatorStatsMutator, CountsRealParses) {

  fs::path instance = dir / "out" / "default";
  ASSERT_NO_FATAL_FAILURE(start("3600", instance));
  string a = write_entry(instance / "queue", "a", kValid);
  string b = write_entry(instance / "queue", "b", kNotUtf8);
  EXPECT_EQ(queue_get(a), 1);
  queue_get(b);
  queue_get(b);  // the rejected set skips the parse
  queue_get((instance / "queue" / "missing").string());  // not read, no parse
  stop();

  // The cache belongs to the process, and nothing parses after the last line
  size_t parser_states = 0, lexer_states = 0;
  ASSERT_TRUE(tree_parser_cache_states(&parser_states, &lexer_states));

  vector<string> lines = read_lines(stats_file(instance));
  ASSERT_EQ(lines.size(), 3u);
  EXPECT_EQ(lines[0], kHeader);
  vector<string> first = split_fields(lines[1]);
  ASSERT_EQ(first.size(), 7u);
  EXPECT_EQ(first[1], "0");

  vector<string> last = split_fields(lines[2]);
  ASSERT_EQ(last.size(), 7u);
  EXPECT_EQ(last[1], "2");
  EXPECT_EQ(last[2], "1");
  EXPECT_EQ(last[3], "0");
  EXPECT_EQ(last[4], std::to_string(parser_states));
  EXPECT_EQ(last[5], std::to_string(lexer_states));
  EXPECT_GT(lexer_states, 0u);  // every parse uses the lexer cache
  EXPECT_NE(std::stoll(last[6]), 0);

}

TEST_F(MutatorStatsMutator, FirstOpenFromQueueNewEntry) {

  // AFL++ calls afl_custom_queue_new_entry for each seed before it calls
  // afl_custom_queue_get, so this call opens the file in a real run
  fs::path instance = dir / "out" / "default";
  ASSERT_NO_FATAL_FAILURE(start("3600", instance));
  string x = write_entry(instance / "queue", "x", kValid);
  afl_custom_queue_new_entry(data, (const uint8_t *)x.c_str(), nullptr);
  stop();

  vector<string> lines = read_lines(stats_file(instance));
  ASSERT_EQ(lines.size(), 3u);
  EXPECT_EQ(split_fields(lines[1])[1], "0");  // the line before the parse
  EXPECT_EQ(split_fields(lines[2])[1], "1");

}

TEST_F(MutatorStatsMutator, TicksFromFuzz) {

  fs::path instance = dir / "out" / "default";
  ASSERT_NO_FATAL_FAILURE(start("1", instance));
  string a = write_entry(instance / "queue", "a", kValid);
  ASSERT_EQ(queue_get(a), 1);
  ASSERT_GT(afl_custom_fuzz_count(data, nullptr, 0), 0u);
  EXPECT_EQ(read_lines(stats_file(instance)).size(), 2u);

  usleep(1100000);
  uint8_t *buf = nullptr;
  afl_custom_fuzz(data, (uint8_t *)kValid.data(), kValid.size(), &buf, nullptr,
                  0, 4096);
  EXPECT_EQ(read_lines(stats_file(instance)).size(), 3u);

}

TEST_F(MutatorStatsMutator, SameOutputsWithStats) {

  vector<uint64_t> hashes[2];
  for (int run = 0; run < 2; ++run) {

    fs::path instance = dir / ("out" + std::to_string(run)) / "default";
    ASSERT_NO_FATAL_FAILURE(start(run ? "3600" : nullptr, instance));
    string a = write_entry(instance / "queue", "a", kValid);
    ASSERT_EQ(queue_get(a), 1);
    uint32_t num = afl_custom_fuzz_count(data, nullptr, 0);
    ASSERT_GT(num, 0u);
    for (uint32_t i = 0; i < num && i < 200; ++i) {

      uint8_t *buf = nullptr;
      size_t   size = afl_custom_fuzz(data, (uint8_t *)kValid.data(),
                                      kValid.size(), &buf, nullptr, 0, 4096);
      hashes[run].push_back(hash_bytes(buf, size));

    }

    stop();

  }

  EXPECT_TRUE(fs::exists(stats_file(dir / "out1" / "default")));
  EXPECT_EQ(hashes[0].size(), hashes[1].size());
  EXPECT_EQ(hashes[0], hashes[1]);

}
