# Grammar Mutator - AFL++

![Grammar Mutator CI](https://github.com/AFLplusplus/Grammar-Mutator/workflows/Grammar%20Mutator%20CI/badge.svg)

A grammar-based custom mutator written for GSoC '20.

- GSoC '20 Developer: [Shengtuo Hu (h1994st)](https://github.com/h1994st)
- GSoC '20 Mentors: [Marc Heuse](https://github.com/vanhauser-thc), [Andrea Fioraldi](https://github.com/andreafioraldi)

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

Note that the grammar mutator is based on the latest custom mutator APIs in AFL++, so please use the latest `dev` branch of [AFL++](https://github.com/AFLplusplus/AFLplusplus).

```bash
git clone https://github.com/AFLplusplus/AFLplusplus.git
cd AFLplusplus
git checkout dev
make distrib
sudo make install
```

### Building Grammar Mutator

Then, you need to build the grammar mutator.
To specify the grammar file, like Ruby, you can use `GRAMMAR_FILE` environment variable.
There are several grammar files in `grammars` directory, such as `json_grammar.json` and `ruby_grammar.json`.
Please refer to [customizing-grammars.md](doc/customizing-grammars.md) for more details about the input grammar file.

```bash
make GRAMMAR_FILE=grammars/ruby_grammar.json \
     ANTLR_JAR_LOCATION=/usr/local/lib/antlr-4.8-complete.jar
```

Now, you should be able to see two symbolic files `libgrammarmutator.so` and `grammar_generator` under the root directory.
These two files actually locate in `src` directory.

If you would like to fork the project and fix bugs or contribute to the project, you can take a look at [building-grammar-mutator.md](doc/building-grammar-mutator.md) for full building instructions.

### Instrumenting Fuzzing targets

You can refer to [sample-fuzzing-targets.md](doc/sample-fuzzing-targets.md) to build the fuzzing targets.

### Fuzzing the Target with the Grammar Mutator!

Before fuzzing the real program, you need to prepare the input fuzzing seeds. You can either:

- Generating seeds for a given grammar
- Using existing seeds

#### Using Generated Seeds

`grammar_generator` can be used to generate input fuzzing seeds and corresponding tree files, following the grammar file that you specified during the compilation of the grammar mutator (i.e., `GRAMMAR_FILE`).
You can control the number of generated seeds and the maximal size of the corresponding trees.
Usually, the larger the tree size is, the more complex the corresponding input seed is.

```bash
./grammar_generator 123 100 1000 ./seeds ./trees

# Usage
# ./grammar_generator <random seed> <max_num> <max_size> <output_dir> <tree_output_dir>
```

Let's start running the fuzzer.
You may notice that the fuzzer will be stuck for a while at the beginning of fuzzing.
One reason for the stuck is the large `max_size` (i.e., 1000) we choose, which results in a large size of test cases.
It will take the fuzzer some time to load them.
Another reason is the costly parsing operations in the grammar mutator.
Since the input seeds are in string format, the grammar mutator needs to parse them into tree representations at first, which is costly.
The large `max_size` passed into `grammar_generator` does help us generate deeply nested trees, but it further increases the parsing overhead.


```bash
export AFL_CUSTOM_MUTATOR_LIBRARY=/path/to/libgrammarmutator.so
export AFL_CUSTOM_MUTATOR_ONLY=1
afl-fuzz -i seeds -o out -- /path/to/target @@
```

To avoid the parsing time, you could feed the generated tree files into the grammar mutator.
In this case, the grammar mutator will directly read trees from files.

```bash
export AFL_CUSTOM_MUTATOR_LIBRARY=/path/to/libgrammarmutator.so
export AFL_CUSTOM_MUTATOR_ONLY=1
mkdir out
cp -r trees out
afl-fuzz -i seeds -o out -- /path/to/target @@
```

#### Using Existing Seeds

Of course, you can feed your own fuzzing seeds to the fuzzer, which does not need to match with your input grammar file.
Assuming that the grammar mutator is built with `grammars/ruby_grammar.json`, which is a simplified Ruby grammar and does not cover all Ruby syntax.
Then, let's use test cases in `mruby` project as input fuzzing seeds.
In this case, the parsing error will definitely occur.
For any parsing errors, the grammar mutator will not terminate but save the error portion as a terminal node in the tree, such that we will not lose too much information on the original test case.

Besides, the default memory limit for child process is `75M` in `afl-fuzz`.
This may not be enough for some test cases, so we increase it to `128M` by adding an option `-m 128M`.

```bash
git clone https://github.com/mruby/mruby.git

export AFL_CUSTOM_MUTATOR_LIBRARY=/path/to/libgrammarmutator.so
export AFL_CUSTOM_MUTATOR_ONLY=1
afl-fuzz -m 128M -i mruby/test/t -o ./out -- /path/to/target @@
```

## Contact & Contributions

We welcome any questions and contributions! Feel free to open an issue or submit a pull request!
