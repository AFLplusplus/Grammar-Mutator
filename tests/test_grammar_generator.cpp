/*
   american fuzzy lop++ - grammar mutator
   --------------------------------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   A grammar-based custom mutator written for GSoC '20.

 */

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include <stdlib.h>

#include "gtest/gtest.h"

namespace fs = std::filesystem;

// GRAMMAR_GENERATOR is the path of the grammar_generator program. The build
// files define it.
class GrammarGeneratorTest : public ::testing::Test {

 protected:
  fs::path dir;

  void SetUp() override {

    std::string tmpl =
        (fs::temp_directory_path() / "grammar_generator_XXXXXX").string();
    ASSERT_NE(mkdtemp(tmpl.data()), nullptr);
    dir = tmpl;

  }

  void TearDown() override {

    fs::remove_all(dir);

  }

  // Run the generator with a fixed random seed. Return its exit code.
  int Run(int max_num, int max_size) {

    std::string cmd = std::string(GRAMMAR_GENERATOR) + " " +
                      std::to_string(max_num) + " " + std::to_string(max_size) +
                      " " + (dir / "seeds").string() + " " +
                      (dir / "trees").string() + " 1 > /dev/null";
    return std::system(cmd.c_str());

  }

  size_t Count(const char *sub) {

    auto it = fs::directory_iterator(dir / sub);
    return std::distance(fs::begin(it), fs::end(it));

  }

  // Read the seeds "0" to "n-1". Fail if a file is missing.
  std::vector<std::string> ReadSeeds() {

    std::vector<std::string> seeds;
    size_t                   n = Count("seeds");
    for (size_t i = 0; i < n; ++i) {

      fs::path path = dir / "seeds" / std::to_string(i);
      EXPECT_TRUE(fs::exists(path)) << path;
      std::ifstream in(path, std::ios::binary);
      seeds.emplace_back(std::istreambuf_iterator<char>(in),
                         std::istreambuf_iterator<char>());

    }

    return seeds;

  }

};

TEST_F(GrammarGeneratorTest, SeedsAreNotEmptyAndNotRepeated) {

  ASSERT_EQ(Run(200, 20), 0);
  std::vector<std::string> seeds = ReadSeeds();
  ASSERT_FALSE(seeds.empty());

  std::set<std::string> unique(seeds.begin(), seeds.end());
  EXPECT_EQ(unique.size(), seeds.size());
  EXPECT_EQ(unique.count(""), 0u);
  EXPECT_EQ(Count("trees"), seeds.size());

}

TEST_F(GrammarGeneratorTest, StopsWhenNoNewSeedIsFound) {

  // With a size of 1, the grammar gives only a few different test cases
  ASSERT_EQ(Run(5000, 1), 0);
  EXPECT_LT(Count("seeds"), 5000u);
  EXPECT_EQ(Count("trees"), Count("seeds"));

}
