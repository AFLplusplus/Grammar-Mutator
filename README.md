# Grammar Mutator - AFL++

![Grammar Mutator CI](https://github.com/AFLplusplus/Grammar-Mutator/workflows/Grammar%20Mutator%20CI/badge.svg)

## Overview

In this project, we developed a grammar mutator to enhance AFL++ such that AFL++ can handle highly-structured inputs, such as JSON, Ruby, etc. The grammar mutator leverages the idea of [F1 fuzzer](https://github.com/vrthra/F1) and [Nautilus](https://github.com/nautilus-fuzz/nautilus) for test case generation and mutations. In summary, this repository includes:

- Tree-based mutation: rules mutation, random mutation, random recursive mutation, splicing mutation
- Tree-based trimming: subtree trimming, recursive trimming
- An ANTLR4 shim for parsing fuzzing test cases during the runtime
- Documents about how to build the grammar mutator, specify custom grammars, and use the grammar mutator
- Comprehensive test cases for unit testing
- Sample grammar files and a script to convert nautilus's python grammar file

For mutation and trimming details, please refer to [Nautilus paper](https://www.syssec.ruhr-uni-bochum.de/media/emma/veroeffentlichungen/2018/12/17/NDSS19-Nautilus.pdf).

## Getting Started

### Prerequisites

Before getting started, the following tools/packages should be installed:

```bash
sudo apt install valgrind uuid-dev default-jre python3
wget https://www.antlr.org/download/antlr-4.8-complete.jar
sudo mv antlr-4.8-complete.jar /usr/local/lib
```

### Building Grammar Mutator

Then, you need to build the grammar mutator. Assuming that you want to have a JSON grammar mutator.

```bash
git clone https://github.com/AFLplusplus/Grammar-Mutator
cd Grammar-Mutator
make ANTLR_JAR_LOCATION=/usr/local/lib/antlr-4.8-complete.jar
```

To specify other grammar files, like Ruby, you can use `GRAMMAR_FILE` environment variable.
There are several grammar files in `grammars` directory, such as `json_grammar.json` and `ruby_grammar.json`.
Please refer to [customizing-grammars.md](doc/customizing-grammars.md) for more details about the input grammar file.

```bash
make GRAMMAR_FILE=grammars/ruby_grammar.json \
     ANTLR_JAR_LOCATION=/usr/local/lib/antlr-4.8-complete.jar
```

Now, you should have `libgrammarmutator.so` under `src` directory

If you would like to fork the project and fix bugs or contribute to the project, you can take a look at [building-grammar-mutator.md](doc/building-grammar-mutator.md) for full building instructions.

### Generating Fuzzing Corpus

Before fuzzing the real program, you need to prepare the input fuzzing seeds.
`grammar_generator` can be used to generate input fuzzing seeds and corresponding tree files.
You can also control the number of generated seeds and the maximal size of the corresponding trees.
Usually, the larger the tree size is, the more complex the corresponding input seed is.

```bash
./grammar_generator 123 100 1000 /tmp/seeds /tmp/trees

# Usage
# ./grammar_generator <random seed> <max_num> <max_size> <output_dir> <tree_output_dir>
```

### Instrumenting Fuzzing targets

You can refer to [sample-fuzzing-targets.md](doc/sample-fuzzing-targets.md) to build the fuzzing targets.

### Fuzzing the Target with the Grammar Mutator!

```bash
export export AFL_CUSTOM_MUTATOR_LIBRARY=/path/to/libgrammarmutator.so
export AFL_CUSTOM_MUTATOR_ONLY=1
afl-fuzz -i /tmp/seeds -o /tmp/out -- /path/to/target @@
```

Since the input seeds are in string format, the grammar mutator needs to parse them into tree representations at first.
To avoid such parsing time, you could feed the generated tree files into the grammar mutator.
In this case, the grammar mutator will directly read trees from files.

```bash
export export AFL_CUSTOM_MUTATOR_LIBRARY=/path/to/libgrammarmutator.so
export AFL_CUSTOM_MUTATOR_ONLY=1
mkdir /tmp/out
cp -r /tmp/trees /tmp/out
afl-fuzz -i /tmp/seeds -o /tmp/out -- /path/to/target @@
```

## Contact & Contributions

We welcome any questions and contributions! Feel free to open an issue or submit a pull request!
