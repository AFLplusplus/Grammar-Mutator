# Grammar Mutator - AFL++

[![Grammar Mutator CI](https://github.com/AFLplusplus/Grammar-Mutator/actions/workflows/main.yml/badge.svg)](https://github.com/AFLplusplus/Grammar-Mutator/actions/workflows/main.yml)

A grammar-based custom mutator for AFL++ to handle highly-structured inputs.

- Developer: [Shengtuo Hu (h1994st)](https://github.com/h1994st)
- Mentors: [Marc Heuse](https://github.com/vanhauser-thc), [Andrea Fioraldi](https://github.com/andreafioraldi)

## Contents

- [Overview](#overview)
- [Quick Start](#quick-start)
- [Prerequisites](#prerequisites)
- [Building the Grammar Mutator](#building-the-grammar-mutator)
- [Seeds](#seeds)
- [Fuzzing](#fuzzing)
- [Configuration](#configuration)
- [Contact and Contributions](#contact-and-contributions)

## Overview

The grammar mutator lets AFL++ fuzz targets that read highly-structured inputs, such as JSON or Ruby.
It does not mutate a byte buffer.
It parses each test case into a tree, changes the tree, and writes the tree back as bytes.
It uses ideas from the [F1 fuzzer](https://github.com/vrthra/F1) for test case generation and from [Nautilus](https://github.com/nautilus-fuzz/nautilus) for the mutations.

This repository includes:

- Grammars in a JSON format or in the ANTLR 4 format (`.g4`), for example from [grammars-v4](https://github.com/antlr/grammars-v4)
- Tree-based mutation: rules mutation, random mutation, random recursive mutation, splicing mutation
- Tree-based trimming: subtree trimming, recursive trimming
- An ANTLR 4 shim that parses test cases while AFL++ runs
- Tools that generate seeds (`grammar_generator`) and parse existing seeds (`grammar_parser`, for `.g4` grammars)
- Sample grammars and a script that converts Nautilus Python grammars
- Unit tests

For more details about tree-based mutation, trimming, and grammar-based fuzzing, refer to the [Nautilus paper](https://www.syssec.ruhr-uni-bochum.de/media/emma/veroeffentlichungen/2018/12/17/NDSS19-Nautilus.pdf).
A fuzzing writeup on Apache that uses the AFL++ Grammar Mutator is at
[https://securitylab.github.com/research/fuzzing-apache-1](https://securitylab.github.com/research/fuzzing-apache-1).

## Quick Start

```bash
make antlr-jar                         # download the ANTLR 4.13.2 jar
make GRAMMAR_FILE=grammars/ruby.json   # build the mutator for a grammar
./grammar_generator-ruby 100 1000 ./seeds ./trees
mkdir -p out/default && cp -r trees out/default

export AFL_CUSTOM_MUTATOR_LIBRARY=./libgrammarmutator-ruby.so
export AFL_CUSTOM_MUTATOR_ONLY=1
afl-fuzz -i seeds -o out -- /path/to/target @@
```

The sections below explain each step.

## Prerequisites

Install these packages:

```bash
sudo apt install valgrind uuid-dev default-jre python3 python3-venv
```

The build needs the ANTLR 4.13.2 jar (Java 11 or later).
In the Grammar-Mutator folder, `make antlr-jar` downloads it.
You can also download it yourself:

```bash
wget https://www.antlr.org/download/antlr-4.13.2-complete.jar
sudo cp -f antlr-4.13.2-complete.jar /usr/local/lib
```

The build looks for the jar in the Grammar-Mutator folder and in `/usr/local/lib`.
If the jar is in another place, add `ANTLR_JAR_LOCATION=...` to the make command.

A `.g4` grammar also needs the Python tools in `.venv`.
Run `make deps` once to install them (this needs `python3-venv` and access to PyPI).

The grammar mutator uses the latest custom mutator APIs of AFL++.
Use the latest `stable` or `dev` branch of [AFL++](https://github.com/AFLplusplus/AFLplusplus):

```bash
git clone https://github.com/AFLplusplus/AFLplusplus.git
cd AFLplusplus
make distrib
sudo make install
```

## Building the Grammar Mutator

Set the grammar with `GRAMMAR_FILE`.
The grammar can be one of two kinds:

- **A JSON grammar file.** There are several in the `grammars` folder, such as `json.json`, `ruby.json`, and `http.json`.
- **An ANTLR 4 grammar (`.g4` files).** You can use a grammar from [grammars-v4](https://github.com/antlr/grammars-v4) directly. Give all its `.g4` files, for example the lexer grammar and the parser grammar. Samples are in `grammars/g4`.

```bash
# A JSON grammar
make GRAMMAR_FILE=grammars/ruby.json

# An ANTLR 4 grammar: one combined grammar ...
make deps
make GRAMMAR_FILE=grammars/g4/JSON.g4

# ... or a lexer grammar and a parser grammar. This grammar has action code,
# so it also needs its C++ base classes (G4_SUPPORT_DIR).
make GRAMMAR_FILE="grammars/g4/javascript/JavaScriptLexer.g4 grammars/g4/javascript/JavaScriptParser.g4" \
     G4_SUPPORT_DIR=grammars/g4/javascript/Cpp
```

Refer to [customizing-grammars.md](doc/customizing-grammars.md) for both kinds of grammar.
Its [ANTLR 4 section](doc/customizing-grammars.md#antlr-4-grammars-g4) gives the limits of `.g4` grammars.
Pull requests with new grammars are welcome! :-)

### Output Files

The build creates these files in the `src` folder, with symbolic links to them in the repository root:

- `libgrammarmutator-<name>.so`: the custom mutator library for AFL++
- `grammar_generator-<name>`: makes seeds and their trees (see [Generating Seeds](#generating-seeds))
- `grammar_parser-<name>`: parses existing seeds into trees, for a `.g4` grammar only (see [Using Existing Seeds](#using-existing-seeds))

The name comes from the grammar file, so you can build the mutator for more than one grammar:

- For a JSON grammar, the build cuts the file name at the first underscore, dash, or dot. So `ruby.json` gives `ruby`.
- For a `.g4` grammar, the name is the last file name without `Lexer` or `Parser`. So the JavaScript example above gives `javascript`.
- To set your own name, add `GRAMMAR_FILENAME=yourname` to the make command.

To fork the project, fix bugs, or contribute, refer to [building-grammar-mutator.md](doc/building-grammar-mutator.md) for the full build instructions.

## Seeds

Before you fuzz the target, prepare the seeds.
You can generate seeds from the grammar, or use existing seeds.
In both cases, you can give the mutator the trees of the seeds, so that it does not parse them again.

### Generating Seeds

`grammar_generator-<name>` makes seeds and their tree files from the grammar of the build.
You can set the number of seeds and the maximum tree size.
A larger tree usually gives a more complex seed.
The generator does not write empty seeds or the same seed twice.
If 100 tries in a row give no new seed, it stops early and writes fewer than `<max_num>` seeds.

```bash
# Usage:
# ./grammar_generator-<name> <max_num> <max_size> <seed_output_dir> <tree_output_dir> [<random seed>]
./grammar_generator-ruby 100 1000 ./seeds ./trees
```

Then [copy the trees](#copying-the-trees).

### Using Existing Seeds

You can give your own seeds to the fuzzer.
They do not have to match the grammar.
The mutator parses each seed when AFL++ loads it.
What happens with a seed that the grammar does not cover depends on the kind of grammar:

- **A JSON grammar:** the mutator keeps the bad part as a terminal node in the tree, so the seed does not lose much information. For example, `grammars/ruby.json` is a simplified Ruby grammar, so most real Ruby files have parse errors.
- **A `.g4` grammar:** a seed with a syntax error gets no tree, and the grammar mutator does not change it. The other AFL++ stages change it, unless `AFL_CUSTOM_MUTATOR_ONLY` is set. With `AFL_CUSTOM_MUTATOR_ONLY=1`, AFL++ does not fuzz this seed at all.

For example, to use the test cases of the `mruby` project as seeds, clone it with `git clone https://github.com/mruby/mruby.git` and give `-i mruby/test/t` to `afl-fuzz`.

With a `.g4` grammar, you can parse the seeds before you fuzz.
This makes the start of `afl-fuzz` faster:

```bash
./grammar_parser-javascript ./seeds ./trees
```

Then [copy the trees](#copying-the-trees).
The [ANTLR 4 section](doc/customizing-grammars.md#parse-existing-test-cases) of customizing-grammars.md gives the skip reasons, the parse time limit, and `G4_SUPPORT_DIR`.

### Copying the Trees

Copy the `trees` folder, with this exact name, into the output folder of `afl-fuzz` (for example `-o out` with the default fuzzer name `default`):

```bash
mkdir -p out/default
cp -r trees out/default
```

If you use more than one fuzzer instance (`-M`/`-S` sync mode), do this for each instance.
For example, for the instances `fuzzer1` to `fuzzer8`:

```bash
for i in 1 2 3 4 5 6 7 8; do
  mkdir -p out/fuzzer$i
  cp -r trees out/fuzzer$i/
done
```

## Fuzzing

### Instrumenting the Target

Refer to [sample-fuzzing-targets.md](doc/sample-fuzzing-targets.md) to build the sample fuzzing targets.

### Running the Fuzzer

This example uses the Ruby grammar (`grammars/ruby.json`) and the `mruby` target:

```bash
export AFL_CUSTOM_MUTATOR_LIBRARY=./libgrammarmutator-ruby.so
export AFL_CUSTOM_MUTATOR_ONLY=1
afl-fuzz -i seeds -o out -- /path/to/target @@
```

The fuzzer can be slow at the start, for two reasons:

- Large seeds take more time to load. A large `<max_size>` for `grammar_generator-<name>` gives large seeds.
- The mutator must parse each seed that has no tree. To skip this step, copy the trees of the seeds (see [Copying the Trees](#copying-the-trees)).

AFL++ adds the mutation stage to the name of each case that a mutation stage finds, in `queue/`, `crashes/`, and `hangs/`: `op:gm-rules`, `op:gm-random`, `op:gm-recursive`, or `op:gm-splice`.

## Configuration

The mutator reads these environment variables:

| Variable | Effect |
|---|---|
| `RANDOM_MUTATION_STEPS` | The number of random mutations (default 1000) |
| `RANDOM_RECURSIVE_MUTATION_STEPS` | The number of random recursive mutations (default 1000) |
| `SPLICING_MUTATION_STEPS` | The number of splicing mutations (default 1000) |
| `RULES_MUTATION_STEPS` | The maximum number of rules mutations (default: no limit) |
| `TRIMMING_STEPS` | The maximum number of trimming steps (default: no limit) |
| `PARSE_TIME_BUDGET` | `0` removes the time limit for the parse of a test case with a `.g4` grammar. Any other value keeps the limit. |

By default, the rules mutation tries every other rule at every nonterminal node of the tree.
With `RULES_MUTATION_STEPS`, it tries at most that many of these node and rule pairs.
It samples them uniformly across the whole tree and tries them in tree order.

The trimming of a new queue entry tries every nonterminal node and every recursion of its tree, from the root down.
With `TRIMMING_STEPS`, it stops after that many tries and keeps what it has trimmed by then.

For example:

```bash
export RANDOM_MUTATION_STEPS=10000
export RANDOM_RECURSIVE_MUTATION_STEPS=10000
export SPLICING_MUTATION_STEPS=10000
export RULES_MUTATION_STEPS=10000
export TRIMMING_STEPS=10000
export AFL_CUSTOM_MUTATOR_LIBRARY=./libgrammarmutator-ruby.so
export AFL_CUSTOM_MUTATOR_ONLY=1
afl-fuzz -i seeds -o out -- /path/to/target @@
```

## Contact and Contributions

We welcome questions and contributions! Feel free to open an issue or submit a pull request.
Read [CONTRIBUTING.md](CONTRIBUTING.md) before you open a pull request.

### Branches and Releases

`stable` is the only long-lived branch.
Send all pull requests to `stable`.

The `dev` branch is abandoned.
It stays in the repository for history, but it does not get new changes.

[Release Please](https://github.com/googleapis/release-please) makes the releases from `stable`.
See [CHANGELOG.md](CHANGELOG.md) and the [GitHub releases](https://github.com/AFLplusplus/Grammar-Mutator/releases) for the changes in each version.
