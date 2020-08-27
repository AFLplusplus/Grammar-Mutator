Building Grammar Mutator
===

## Prerequisites

To build the grammar mutator, the following tools/packages are required

```bash
sudo apt install valgrind uuid-dev default-jre python3
wget https://www.antlr.org/download/antlr-4.8-complete.jar
sudo mv antlr-4.8-complete.jar /usr/local/lib
```

## Get the source code

The first step is to get the source code from the Grammar Mutator repository at Github.
You can also download the repository from Github.

```bash
git clone https://github.com/AFLplusplus/Grammar-Mutator
```

## Compile

We support both GNU Makefile and CMake. For both building scripts, we have several options.
After successfully compiling the grammar mutator, you should have `libgrammarmutator.so` and `grammar_generator` under `src` directory.

```
ENABLE_TESTING - compiles test cases
ENABLE_DEBUG - compiles with '-g' option for debug purposes
GRAMMAR_FILE - the path to the input grammar file
               (Default: grammars/json_grammar.json)
ANTLR_JAR_LOCATION - the path to ANTLR4 jar file
```

### Makefile

```bash
make ENABLE_TESTING=1 ANTLR_JAR_LOCATION=/usr/local/lib/antlr-4.8-complete.jar GRAMMAR_FILE=grammars/ruby_grammar.json
make test
make test_memcheck  # if with Valgrind installed
```

### CMake

```bash
mkdir build
cd build
cmake -DENABLE_TESTING=ON -DANTLR_JAR_LOCATION=/usr/local/lib/antlr-4.8-complete.jar \
      -DGRAMMAR_FILE=$(realpath ../grammars/ruby_grammar.json) ../
make test
make test_memcheck  # if with Valgrind installed
```
