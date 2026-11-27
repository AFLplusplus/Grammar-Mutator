# Benchmark results

This folder holds recorded results of the benchmark program. A result is a
comparison point for a later performance change.

Each result has its own folder. The folder name is the date of the run, as
`YYYY-MM-DD`. A second result on the same day adds `-2`, a third adds `-3`,
and so on. Do not make a folder for each release. Record a result when you
need a comparison point.

## What a result folder holds

- `README.md` with these items:
  - what the result measured: the commit, and the release whose library code
    it has;
  - the build commands and the build type;
  - the seeds: the source, the version, the count, and the command that made
    them;
  - the exact benchmark commands;
  - a generated part between two marker lines: the platform, the wall time of
    each command, and the tables of the main numbers. `src/benchmark/report.py`
    writes it from the `.txt` files.
- The raw output of each command, in one `.txt` file for each command.
- `platform.txt`: the output of `src/benchmark/platform.sh`.

## How to record a result

The program `benchmark-<name>` is in `src/benchmark/` after a make build, and
in `<build>/src/benchmark/` after a CMake build.

1. Build with optimization. Use make, or CMake with the default
   `RelWithDebInfo` build type. See
   [building-grammar-mutator.md](../building-grammar-mutator.md).
2. Make the seeds. For the JavaScript `.g4` grammar, download the JerryScript
   test files:

   ```bash
   src/benchmark/get-jerryscript-seeds.sh jerry-seeds
   ```

   For any grammar, use the generator:

   ```bash
   grammar_generator-<name> 100 1000 jerry-seeds trees 1
   ```

   Both commands make the folder `jerry-seeds`. The benchmark commands below
   use this folder. Use your own folder name if you prefer, and use it in
   every command.

3. Save the platform facts:

   ```bash
   src/benchmark/platform.sh > platform.txt
   ```

4. Run the benchmark commands. Save the output of each command in its own
   file:

   ```bash
   benchmark-<name> run -e 5 -n 5000 -t 2000 jerry-seeds > run.txt
   benchmark-<name> digest -n 200 -t 20 jerry-seeds > digest.txt
   ```

5. Make the result folder `doc/benchmarks/<date>/`. Put the `.txt` files in
   it. Write the hand-written parts of `README.md`, and end them with these
   two marker lines:

   ```
   <!-- report.py: start of the generated part -->
   <!-- report.py: end of the generated part -->
   ```

6. Write the generated part between the markers:

   ```bash
   src/benchmark/report.py doc/benchmarks/<date>
   ```

   The script replaces only the text between the markers. Add `--check` to
   see if the file is up to date. `make test-python` checks every committed
   result with the markers. Do not edit the generated part by hand.
7. Add a line for the folder to the list below.

## Results

- [2026-11-27](2026-11-27/README.md): the library of release 1.2.0, with the
  JavaScript `.g4` grammar and `ruby.json`, on an Apple M4 with macOS 26.6.2.
