# Grammar Mutator - AFL++

![Grammar Mutator CI](https://github.com/AFLplusplus/Grammar-Mutator/workflows/Grammar%20Mutator%20CI/badge.svg)

A grammar-based custom mutator for AFL++ to handle highly-structured inputs.

- Developer: [Shengtuo Hu (h1994st)](https://github.com/h1994st)
- Mentors: [Marc Heuse](https://github.com/vanhauser-thc), [Andrea Fioraldi](https://github.com/andreafioraldi)

## Overview

We developed a grammar mutator to enhance AFL++ such that AFL++ can handle highly-structured inputs, such as JSON, Ruby, etc.
The grammar mutator leverages the idea of [F1 fuzzer](https://github.com/vrthra/F1) and [Nautilus](https://github.com/nautilus-fuzz/nautilus) for test case generation and mutations.
In summary, this repository includes:

- Tree-based mutation: rules mutation, random mutation, random recursive mutation, splicing mutation
- Tree-based trimming: subtree trimming, recursive trimming
- An ANTLR4 shim for parsing fuzzing test cases during the runtime
- Documents about how to build the grammar mutator, specify custom grammars, and use the grammar mutator
- Comprehensive test cases for unit testing
- Sample grammar files and a script to convert nautilus's python grammar file

For more details about tree-based mutation, trimming, and grammar-based fuzzing, please refer to [Nautilus paper](https://www.syssec.ruhr-uni-bochum.de/media/emma/veroeffentlichungen/2018/12/17/NDSS19-Nautilus.pdf).

A fuzzing writeup on Apache which uses the AFL++ Grammmar Mutator can be found here:
[https://securitylab.github.com/research/fuzzing-apache-1](https://securitylab.github.com/research/fuzzing-apache-1)

## Getting Started

### Prerequisites

Before getting started, the following tools/packages should be installed:

```bash
sudo apt install valgrind uuid-dev default-jre python3
wget https://www.antlr.org/download/antlr-4.8-complete.jar
sudo cp -f antlr-4.8-complete.jar /usr/local/lib
```
If you do not leave the JAR file in the Grammar-Mutator directory or do not copy
it to /usr/local/lib then you must specify the location via ANTLR_JAR_LOCATION=...
in the make command.

Note that the grammar mutator is based on the latest custom mutator APIs in AFL++, so please use the latest `dev` or `stable` branch of [AFL++](https://github.com/AFLplusplus/AFLplusplus/tree/dev).

```bash
git clone https://github.com/AFLplusplus/AFLplusplus.git
cd AFLplusplus
make distrib
sudo make install
```

### Building the Grammar Mutator

Next you need to build the grammar mutator.
To specify the grammar file, eg. Ruby, you can use `GRAMMAR_FILE` environment variable.
There are several grammar files in `grammars` directory, such as `json.json`, `ruby.json` and `http.json`.
Please refer to [customizing-grammars.md](doc/customizing-grammars.md) for more details about the input grammar file.
Note that pull requests with new grammars are welcome! :-)

```bash
make GRAMMAR_FILE=grammars/ruby.json
```

Note that the shared library and grammar generator are named after the grammar file that is specified so you can have multiple grammars generated.
The grammar name part is based on the filename with everything cut off after a underline, dash or dot, hency `ruby.json` will result in `ruby` and hence `grammar_generator-ruby` and `libgrammarmutator-ruby.so` will be created.
You can specify your own naming by setting `GRAMMAR_FILENAME=yourname` as make option.

Now, you should be able to see two symbolic files `libgrammarmutator-ruby.so` and `grammar_generator-ruby` under the root directory.
These two files actually locate in the `src` directory.

If you would like to fork the project and fix bugs or contribute to the project, you can take a look at [building-grammar-mutator.md](doc/building-grammar-mutator.md) for full building instructions.

### Instrumenting Fuzzing Targets

You can refer to [sample-fuzzing-targets.md](doc/sample-fuzzing-targets.md) to build the example fuzzing targets.

### Seeds

Before fuzzing the real program, you need to prepare the input fuzzing seeds. You can either:

- Generating seeds for a given grammar
- Using existing seeds

#### Using Existing Seeds

You can feed your own fuzzing seeds to the fuzzer, which does not need to match with your input grammar file.
Assuming that the grammar mutator is built with `grammars/ruby.json`, which is a simplified Ruby grammar and does not cover all Ruby syntax.
In this case, the parsing error will definitely occur.
For any parsing errors, the grammar mutator will not terminate but save the error portion as a terminal node in the tree, such that we will not lose too much information on the original test case.

To e.g. use the test cases of the `mruby` project as input fuzzing seeds just pass the `-i mruby/test/t` to afl-fuzz
when we run the fuzzer (if it has been checked out with `git clone https://github.com/mruby/mruby.git` in the current directory).

#### Using Generated Seeds

`grammar_generator` can be used to generate input fuzzing seeds and corresponding tree files, following the grammar file that you specified during the compilation of the grammar mutator (i.e., `GRAMMAR_FILE`).
You can control the number of generated seeds and the maximal size of the corresponding trees.
Usually, the larger the tree size is, the more complex the corresponding input seed is.

```bash
# Usage
# ./grammar_generator-$GRAMMAR <max_num> <max_size> <seed_output_dir> <tree_output_dir> [<random seed>]
#
# <random seed> is optional
# e.g.:
./grammar_generator-ruby 100 1000 ./seeds ./trees
```

Afterwards copy the `trees` folder with that exact name to the output directory that you will use with afl-fuzz (e.g. `-o out -S default`):
```bash
mkdir -p out/default
cp -r trees out/default
```

Note that if you use multiple fuzzers (-M/-S sync mode) then you have to do this for all fuzzer instances, e.g. when the fuzzer instances are named fuzzer1 to fuzzer8:
```bash
for i in 1 2 3 4 5 6 7 8; do
  mkdir -p out/fuzzer$i
  cp -r trees out/fuzzer$i/
done
```

### Fuzzing the Target with the Grammar Mutator!

Let's start running the fuzzer.
The following example command is using Ruby grammar (from `grammars/ruby.json`) where `mruby` project has been cloned to the root `Grammar-Mutator` directory.

The default memory limit for child process is `75M` in `afl-fuzz`.
This may not be enough for some test cases, so it is recommended to increase it to `128M` by adding an option `-m 128`.

```bash
export AFL_CUSTOM_MUTATOR_LIBRARY=./libgrammarmutator-ruby.so
export AFL_CUSTOM_MUTATOR_ONLY=1
afl-fuzz -m 128 -i seeds -o out -- /path/to/target @@
```

You may notice that the fuzzer will be stuck for a while at the beginning of fuzzing.
One reason for the stuck is the large `max_size` (i.e., 1000) we choose, which results in a large size of test cases that increases the loading time.
Another reason is the costly parsing operations in the grammar mutator.
Since the input seeds are in string format, the grammar mutator needs to parse them into tree representations at first, which is costly.
The large `max_size` passed into `grammar_generator-$GRAMMAR` does help us generate deeply nested trees, but it further increases the parsing overhead.

### Changing the Default Configurations

Except for the deterministic rules mutation, users can change the default number of the following three types of mutations, by setting related environment variables:

- `RANDOM_MUTATION_STEPS`: the number of random mutations
- `RANDOM_RECURSIVE_MUTATION_STEPS`: the number of random recursive mutations
- `SPLICING_MUTATION_STEPS`: the number of splicing mutations

By default, the number of each of these three mutations is 1000. Increase them on your own as follows, if needed. :)

```bash
export RANDOM_MUTATION_STEPS=10000
export RANDOM_RECURSIVE_MUTATION_STEPS=10000
export SPLICING_MUTATION_STEPS=10000
export AFL_CUSTOM_MUTATOR_LIBRARY=./libgrammarmutator-ruby.so
export AFL_CUSTOM_MUTATOR_ONLY=1
afl-fuzz -m 128 -i seeds -o out -- /path/to/target @@
```

## Contact & Contributions

We welcome any questions and contributions! Feel free to open an issue or submit a pull request!
