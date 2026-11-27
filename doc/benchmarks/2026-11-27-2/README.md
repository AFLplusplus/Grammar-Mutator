# Benchmark result of 2026-11-27 (second)

This result measures the growth of the ANTLR parse cache.

## What it measured

- The library and the benchmark program of this pull request's branch, on top
  of `494f756` on `stable`. It adds the `cache` mode.
- The seeds and the machine are the same as in the result of
  [2026-11-27](../2026-11-27/README.md).
- The load average was about 2.5 before the runs and about 1.9 after them.
  Other desktop programs were open. No build ran at the same time.

## Result

- javascript: the heap grows by 70,588 KB per 10,000 parses. At 1,000,000
  parses this is about 6,900 MB, more than 100 MB. The parse cache needs a
  limit.
- ruby: the heap grows by 50 KB per 10,000 parses. At 1,000,000 parses this is
  about 5 MB, less than 100 MB. The parse cache needs no limit.

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
src/benchmark/platform.sh > platform.txt
build-js/src/benchmark/benchmark-javascript cache -m 100000 jerry-seeds > javascript-cache.txt
build-bench/src/benchmark/benchmark-ruby cache -m 100000 ruby-seeds > ruby-cache.txt
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
| javascript | cache | `-m 100000 -i 10000` | 1,035 | – | 324.6 s |
| ruby | cache | `-m 100000 -i 10000` | 300 | – | 6.9 s |

## Parse time

The tables show the time of the first load of each seed in µs. This time
includes the parse, the tree file write, and the chunk store add. All seeds
count, including the seeds without a tree. The time of the first seed in name
order also includes the one-time setup of the parser.

No run in this result.

## Parse cache

| Grammar | Phase | Parses | Parser states | New parser states | Lexer states | Heap in use | Heap growth | Peak memory | µs per parse | Trees | Not UTF-8 | Stopped |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| javascript | seeds | 1,035 | 14,259 | – | 349 | 562,965 KB | – | 740,064 KB | 19,850 | 1,011 | 0 | 2 |
| javascript | mutants | 11,035 | 16,036 | 1,777 | 355 | 628,141 KB | 65,176 KB | 771,424 KB | 10,822 | 1,992 | 7,224 | 3 |
| javascript | mutants | 21,035 | 17,511 | 1,475 | 357 | 706,220 KB | 78,079 KB | 848,128 KB | 10,025 | 2,933 | 14,384 | 3 |
| javascript | mutants | 31,035 | 18,689 | 1,178 | 359 | 767,546 KB | 61,326 KB | 908,016 KB | 10,400 | 3,867 | 21,576 | 3 |
| javascript | mutants | 41,035 | 19,600 | 911 | 360 | 806,162 KB | 38,616 KB | 945,264 KB | 10,224 | 4,848 | 28,711 | 5 |
| javascript | mutants | 51,035 | 21,422 | 1,822 | 363 | 887,190 KB | 81,028 KB | 1,023,520 KB | 11,213 | 5,777 | 35,989 | 8 |
| javascript | mutants | 61,035 | 22,703 | 1,281 | 363 | 935,082 KB | 47,892 KB | 1,071,248 KB | 11,677 | 6,715 | 43,227 | 9 |
| javascript | mutants | 71,035 | 24,528 | 1,825 | 363 | 1,011,662 KB | 76,580 KB | 1,145,088 KB | 11,244 | 7,678 | 50,414 | 11 |
| javascript | mutants | 81,035 | 25,997 | 1,469 | 364 | 1,075,433 KB | 63,771 KB | 1,207,872 KB | 10,936 | 8,628 | 57,583 | 13 |
| javascript | mutants | 91,035 | 27,546 | 1,549 | 365 | 1,159,748 KB | 84,315 KB | 1,290,528 KB | 11,045 | 9,542 | 64,832 | 14 |
| javascript | mutants | 101,035 | 28,676 | 1,130 | 365 | 1,216,609 KB | 56,861 KB | 1,349,856 KB | 10,432 | 10,509 | 72,016 | 15 |
| ruby | seeds | 300 | 25 | – | 1,963 | 5,363 KB | – | 9,088 KB | 46 | 300 | 0 | 0 |
| ruby | mutants | 10,300 | 25 | 0 | 2,138 | 5,947 KB | 584 KB | 10,272 KB | 225 | 3,183 | 7,117 | 0 |
| ruby | mutants | 20,300 | 25 | 0 | 2,193 | 6,112 KB | 165 KB | 10,448 KB | 216 | 6,106 | 14,194 | 0 |
| ruby | mutants | 30,300 | 25 | 0 | 2,244 | 6,235 KB | 123 KB | 10,640 KB | 219 | 9,026 | 21,274 | 0 |
| ruby | mutants | 40,300 | 25 | 0 | 2,275 | 6,319 KB | 84 KB | 10,720 KB | 217 | 11,999 | 28,301 | 0 |
| ruby | mutants | 50,300 | 25 | 0 | 2,302 | 6,413 KB | 94 KB | 10,832 KB | 230 | 14,903 | 35,397 | 0 |
| ruby | mutants | 60,300 | 25 | 0 | 2,321 | 6,460 KB | 47 KB | 10,880 KB | 220 | 17,842 | 42,458 | 0 |
| ruby | mutants | 70,300 | 25 | 0 | 2,329 | 6,508 KB | 48 KB | 10,912 KB | 220 | 20,769 | 49,531 | 0 |
| ruby | mutants | 80,300 | 25 | 0 | 2,344 | 6,568 KB | 60 KB | 10,960 KB | 219 | 23,655 | 56,645 | 0 |
| ruby | mutants | 90,300 | 25 | 0 | 2,352 | 6,625 KB | 57 KB | 11,056 KB | 218 | 26,574 | 63,726 | 0 |
| ruby | mutants | 100,300 | 26 | 1 | 2,361 | 6,667 KB | 42 KB | 11,136 KB | 220 | 29,482 | 70,818 | 0 |

- javascript: heap growth over the last quarter of the mutants: 70,588 KB per 10,000 parses.
- ruby: heap growth over the last quarter of the mutants: 50 KB per 10,000 parses.

The parser states and the lexer states only grow. The heap in use is the memory in use after each tree is freed. The growth line feeds the decision on clearing the cache. – means that the heap size is not known.

## Time per output

Time in µs for each output, for each stage.

No run in this result.

## Duplicates

Duplicate outputs and outputs equal to the seed, in percent.

No run in this result.

## Memory

No run in this result.

## Digests

No digest in this result.

The duplicate rates go up with the number of steps. Compare results only at the same options and on the same platform. – means that the raw file has no record for the cell. An entry without a recursion edge has no recursive stage.

<!-- report.py: end of the generated part -->
