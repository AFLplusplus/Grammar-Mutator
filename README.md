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

## Getting Started

### Prerequisites

Before getting started, the following tools/packages should be installed:

```bash
sudo apt install valgrind uuid-dev default-jre python3
wget https://www.antlr.org/download/antlr-4.8-complete.jar
sudo mv antlr-4.8-complete.jar /usr/local/lib
```

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
There are several grammar files in `grammars` directory, such as `JSON.json` and `Ruby.json`.
Please refer to [customizing-grammars.md](doc/customizing-grammars.md) for more details about the input grammar file.
Note that pull requests with new grammars are welcome! :-)

```bash
make GRAMMAR_FILE=grammars/Ruby.json \
     ANTLR_JAR_LOCATION=/usr/local/lib/antlr-4.8-complete.jar
```

Now, you should be able to see two symbolic files `libgrammarmutator-Ruby.so` and `grammar_generator` under the root directory.
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
Assuming that the grammar mutator is built with `grammars/Ruby.json`, which is a simplified Ruby grammar and does not cover all Ruby syntax.
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
# ./grammar_generator <random seed> <max_num> <max_size> <seed_output_dir> <tree_output_dir>
# eg:
./grammar_generator 123 100 1000 ./seeds ./trees
```

Afterwards copy the `trees` folder with that exact name to the output directory that you will use with afl-fuzz (e.g. `-o out`):
```bash
mkdir out
cp -r trees out
```

### Fuzzing the Target with the Grammar Mutator!

Let's start running the fuzzer.

You may notice that the fuzzer will be stuck for a while at the beginning of fuzzing.
One reason for the stuck is the large `max_size` (i.e., 1000) we choose, which results in a large size of test cases that increases the loading time.
Another reason is the costly parsing operations in the grammar mutator.
Since the input seeds are in string format, the grammar mutator needs to parse them into tree representations at first, which is costly.
The large `max_size` passed into `grammar_generator` does help us generate deeply nested trees, but it further increases the parsing overhead.

The default memory limit for child process is `75M` in `afl-fuzz`.
This may not be enough for some test cases, so it is recommended to increase it to `128M` by adding an option `-m 128`.

```bash
export AFL_CUSTOM_MUTATOR_LIBRARY=/path/to/libgrammarmutator.so
export AFL_CUSTOM_MUTATOR_ONLY=1
afl-fuzz -m 128 -i seeds -o out -- /path/to/target @@
```

## Contact & Contributions

We welcome any questions and contributions! Feel free to open an issue or submit a pull request!
