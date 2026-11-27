# Benchmark result of 2026-11-27

This result is the comparison point for release 1.2.0.

## What it measured

- The benchmark program of pull request #85, on top of `fc77333` on
  `stable`. It adds the wall-time record (`elapsed`). The library code is the
  same as release 1.2.0 (`v1.2.0`, `9f8df44`).
- The load average was about 2 before and after the runs. Other desktop
  programs were open. No build ran at the same time.

## Build

Both builds use CMake with the `RelWithDebInfo` build type.

```bash
# JavaScript .g4 grammar
cmake -S . -B build-js -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  "-DGRAMMAR_FILE=$PWD/grammars/g4/javascript/JavaScriptLexer.g4;$PWD/grammars/g4/javascript/JavaScriptParser.g4" \
  -DG4_SUPPORT_DIR=$PWD/grammars/g4/javascript/Cpp \
  -DANTLR_JAR_LOCATION=$PWD/antlr-4.13.2-complete.jar
cmake --build build-js -j8

# ruby.json (RelWithDebInfo is the default)
cmake -S . -B build-bench -DGRAMMAR_FILE=$PWD/grammars/ruby.json \
  -DANTLR_JAR_LOCATION=$PWD/antlr-4.13.2-complete.jar
cmake --build build-bench -j8
```

## Seeds

- JavaScript: 1,035 files from the JerryScript v3.0.0 tests. 1,013 of them
  parse into a tree.

  ```bash
  src/benchmark/get-jerryscript-seeds.sh jerry-seeds
  ```

- `ruby.json`: 300 generated files, 131 to 344 bytes. All of them parse into
  a tree.

  ```bash
  build-bench/src/grammar_generator-ruby 300 1000 ruby-seeds ruby-trees 1
  ```

## Commands

```bash
build-js/src/benchmark/benchmark-javascript run -e 5 -n 5000 -t 2000 jerry-seeds > javascript-run.txt
build-js/src/benchmark/benchmark-javascript digest -n 200 -t 20 jerry-seeds > javascript-digest.txt
build-bench/src/benchmark/benchmark-ruby run -e 5 -n 5000 -t 2000 ruby-seeds > ruby-run.txt
build-bench/src/benchmark/benchmark-ruby digest -n 1000 -t 100 ruby-seeds > ruby-digest.txt
```

<!-- report.py: start of the generated part -->

## Platform

```
date: 2026-11-27
system: macOS 26.6.2
kernel: Darwin 25.6.0
machine: arm64
cpu: Apple M4
cores: 10 physical, 10 logical (4 performance, 6 efficiency)
memory: 24.0 GiB
cc: Apple clang version 21.0.0 (clang-2100.1.1.101)
c++: Apple clang version 21.0.0 (clang-2100.1.1.101)
cmake: cmake version 4.2.1
power: Now drawing from 'AC Power'
```

## Runs

| Grammar | Mode | Options | Seeds | Trees | Wall time |
|---|---|---|---|---|---|
| javascript | run | `-e 5 -n 5000 -t 2000` | 1,035 | 1,013 | 131.6 s |
| javascript | digest | `-n 200 -t 20` | 1,035 | 1,013 | 178.9 s |
| ruby | run | `-e 5 -n 5000 -t 2000` | 300 | 300 | 445.9 ms |
| ruby | digest | `-n 1000 -t 100` | 300 | 300 | 10.5 s |

## Parse time

The tables show the time of the first load of each seed in µs. This time
includes the parse, the tree file write, and the chunk store add. All seeds
count, including the seeds without a tree. The time of the first seed in name
order also includes the one-time setup of the parser.

| Grammar | Seeds | No tree | Median | 90th percentile | Slowest | Slowest seed | Median µs per KB |
|---|---|---|---|---|---|---|---|
| javascript | 1,035 | 22 | 6,113 | 41,350 | 1,785,336 | regression-test-issue-453.js | 5,298 |
| ruby | 300 | 0 | 82 | 118 | 1,436 | 0 | 492 |

| Grammar | Size | Seeds | Median |
|---|---|---|---|
| javascript | under 1 KB | 529 | 2,382 |
| javascript | 1–10 KB | 497 | 17,162 |
| javascript | 10–100 KB | 9 | 170,754 |
| ruby | under 1 KB | 300 | 82 |

## Parse cache

No cache run in this result.

## Time per output

Time in µs for each output, for each stage.

| Grammar | Entry | Bytes | Nodes | Rules | Random | Recursive | Splice | Trim (µs/step) |
|---|---|---|---|---|---|---|---|---|
| javascript | regression-test-issue-1821.js | 635 | 19 | 30 | 7.6 | – | 74 | 11 |
| javascript | regression-test-issue-3880.js | 767 | 263 | 37 | 15 | 23 | 60 | 9.4 |
| javascript | regression-test-issue-739.js | 985 | 632 | 52 | 31 | 41 | 64 | 23 |
| javascript | try-pattern.js | 2,077 | 1,972 | 108 | 83 | 92 | 127 | 85 |
| javascript | test_suite_11.js | 78,546 | 121,574 | 4,645 | 4,616 | 4,689 | 4,906 | 5,152 |
| ruby | 0 | 131 | 3 | 6.0 | 0.9 | – | 1.8 | 1.0 |
| ruby | 199 | 150 | 19 | 1.2 | 1.0 | 5.5 | 1.6 | 0.8 |
| ruby | 196 | 167 | 35 | 1.1 | 1.3 | 12 | 1.9 | 1.1 |
| ruby | 134 | 194 | 70 | 2.1 | 2.3 | 13 | 2.9 | 2.2 |
| ruby | 244 | 344 | 175 | 5.4 | 5.6 | 15 | 6.4 | 5.5 |

## Duplicates

Duplicate outputs and outputs equal to the seed, in percent.

| Grammar | Entry | Rules dup % | Rules same % | Random dup % | Random same % | Recursive dup % | Recursive same % | Splice dup % | Splice same % |
|---|---|---|---|---|---|---|---|---|---|
| javascript | regression-test-issue-1821.js | 1.3 | 0.0 | 40.1 | 12.8 | – | – | 19.6 | 6.7 |
| javascript | regression-test-issue-3880.js | 0.6 | 0.3 | 41.5 | 13.6 | 97.0 | 0.1 | 15.8 | 11.1 |
| javascript | regression-test-issue-739.js | 0.7 | 0.7 | 31.1 | 12.7 | 94.5 | 0.0 | 11.9 | 8.2 |
| javascript | try-pattern.js | 0.7 | 0.6 | 25.6 | 14.6 | 88.3 | 0.0 | 10.4 | 7.9 |
| javascript | test_suite_11.js | 0.7 | 0.7 | 15.7 | 15.4 | 4.7 | 0.0 | 8.6 | 8.6 |
| ruby | 0 | 0.0 | 0.0 | 66.7 | 49.8 | – | – | 85.3 | 0.3 |
| ruby | 199 | 0.0 | 2.6 | 78.6 | 28.3 | 99.7 | 0.0 | 67.2 | 22.7 |
| ruby | 196 | 0.4 | 0.5 | 75.5 | 26.4 | 99.5 | 0.0 | 62.6 | 18.4 |
| ruby | 134 | 0.3 | 0.3 | 70.9 | 25.2 | 99.4 | 0.1 | 55.3 | 16.6 |
| ruby | 244 | 0.3 | 0.3 | 66.0 | 28.8 | 98.1 | 0.0 | 46.2 | 22.9 |

## Memory

| Grammar | Chunks | Trees | Fill time | Peak memory |
|---|---|---|---|---|
| javascript | 365,859 | 1,013 | 22.9 s | 850,544 KB |
| ruby | 2,302 | 300 | 28.1 ms | 9,488 KB |

## Digests

| Grammar | Options | Outputs | New entries | Trim steps | Hash |
|---|---|---|---|---|---|
| javascript | `-n 200 -t 20` | 1,613,412 | 1,013 | 36,887 | `14d12669df4be2e1` |
| ruby | `-n 1000 -t 100` | 2,216,315 | 300 | 13,702 | `fd87045834903cd1` |

The duplicate rates go up with the number of steps. Compare results only at the same options and on the same platform. – means that the raw file has no record for the cell. An entry without a recursion edge has no recursive stage.

<!-- report.py: end of the generated part -->
