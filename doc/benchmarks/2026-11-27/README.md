# Benchmark result of 2026-11-27

This result is the comparison point for release 1.2.0.

## What it measured

- Commit `fc77333` on `stable`. Its library code is the same as release
  1.2.0 (`v1.2.0`, `9f8df44`). Only the benchmark program and its build lines
  are different.
- The load average was about 2 before and after the runs. Other desktop
  programs were open. No build ran at the same time.

## Platform

The output of `src/benchmark/platform.sh` (also in `platform.txt`):

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

The wall times were 2 min 12 s, 3 min, 1 s, and 10 s.

## Summary

### JavaScript `.g4`

Time per output in µs, for each stage. The smallest entry has no recursion
edge, so it has no recursive stage.

| Entry | Bytes | Nodes | Rules | Random | Recursive | Splice | Trim (µs/step) |
|---|---|---|---|---|---|---|---|
| regression-test-issue-1821.js | 635 | 19 | 31 | 8 | – | 69 | 10 |
| regression-test-issue-3880.js | 767 | 263 | 36 | 14 | 23 | 62 | 10 |
| regression-test-issue-739.js | 985 | 632 | 49 | 31 | 48 | 72 | 23 |
| try-pattern.js | 2,077 | 1,972 | 102 | 78 | 93 | 124 | 84 |
| test_suite_11.js | 78,546 | 121,574 | 4,641 | 4,574 | 4,685 | 4,862 | 5,162 |

- Chunk store: 365,859 chunks from 1,013 trees. The fill took 22.9 s.
- Peak memory: 855,632 KB.
- Duplicate outputs: rules at most 1.3%, random 16–42%, recursive 88–97%
  (4.7% on the largest entry), splice 9–20%.
- Outputs equal to the seed: random 13–15%, splice 7–11%.

### `ruby.json`

| Entry | Bytes | Nodes | Rules | Random | Recursive | Splice | Trim (µs/step) |
|---|---|---|---|---|---|---|---|
| 0 | 131 | 3 | 9.0 | 1.0 | – | 2.1 | 0.5 |
| 199 | 150 | 19 | 1.3 | 1.0 | 5.5 | 1.6 | 0.8 |
| 196 | 167 | 35 | 1.1 | 1.4 | 12.2 | 1.9 | 1.2 |
| 134 | 194 | 70 | 2.1 | 2.3 | 13.3 | 2.8 | 2.2 |
| 244 | 344 | 175 | 5.5 | 5.7 | 15.1 | 6.5 | 5.7 |

- Chunk store: 2,302 chunks from 300 trees. Peak memory: 9,600 KB.
- Duplicate outputs: random 66–79%, recursive 98–99.7%, splice 46–85%.
- Outputs equal to the seed: random 25–50%, splice 0.3–23%.

### Digests

| Seeds | Options | Outputs | New entries | Trim steps | Hash |
|---|---|---|---|---|---|
| JavaScript | `-n 200 -t 20` | 1,613,412 | 1,013 | 36,887 | `14d12669df4be2e1` |
| `ruby.json` | `-n 1000 -t 100` | 2,216,315 | 300 | 13,702 | `fd87045834903cd1` |

### Notes

- The duplicate rates go up with the number of steps, because a small tree
  has few different outputs. Compare them only at the same `-n`.
- Time per output grows with the tree size. For the largest entry, it is
  about 40 ns for each node.
