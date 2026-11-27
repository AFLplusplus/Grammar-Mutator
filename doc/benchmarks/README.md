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
  - the platform and the hardware: the output of `src/benchmark/platform.sh`;
  - the build commands and the build type;
  - the seeds: the source, the version, the count, and the command that made
    them;
  - the exact benchmark commands;
  - a short summary of the main numbers.
- The raw output of each command, in one `.txt` file for each command.

## How to record a result

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
   grammar_generator-<name> 100 1000 seeds trees 1
   ```

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

5. Make the result folder `doc/benchmarks/<date>/`. Put the `.txt` files and
   a `README.md` with the items above in it.
6. Add a line for the folder to the list below.

## Results

No result yet.
