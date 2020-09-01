Building Grammar Mutator
===

## Prerequisites

To build the grammar mutator, the following tools/packages are required

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

## Get the source code

The first step is to get the source code from the Grammar Mutator repository at Github.
You can also download the repository from Github.

```bash
git clone https://github.com/AFLplusplus/Grammar-Mutator
```

## Compile

We support both GNU Makefile and CMake. For both building scripts, we have several options.

```
ENABLE_TESTING - compiles test cases
ENABLE_DEBUG - compiles with '-g' option for debug purposes
GRAMMAR_FILE - the path to the input grammar file
               (Default: grammars/json_grammar.json)
GRAMMAR_FILENAME - name that will be used in the naming of the generated grammar
                   files, e.g. "ruby" => ./grammar_generator-ruby
ANTLR_JAR_LOCATION - the path to ANTLR4 jar file
```

Note that the shared library and grammar generator are named after the grammar file that is specified so you can have multiple grammars generated.
The grammar name part is based on the filename with everything cut off after a underline, dash or dot, hency `ruby.json` will result in `ruby` and hence `grammar_generator-ruby` and `libgrammarmutator-ruby.so` will be created.
You can specify your own naming by setting `GRAMMAR_FILENAME=yourname` as make option.
After successfully compiling the grammar mutator, you should have `libgrammarmutator-$GRAMMAR.so` and `grammar_generator-$GRAMMAR` under `src` directory.

### Makefile

```bash
make ENABLE_TESTING=1 \
     ANTLR_JAR_LOCATION=/usr/local/lib/antlr-4.8-complete.jar \
     GRAMMAR_FILE=grammars/ruby.json \
     GRAMMAR_FILENAME=ruby
# Generated targets: libgrammarmutator-ruby.so, grammar_generator-ruby
make test
make test_memcheck  # if with Valgrind installed
```

### CMake

```bash
mkdir build
cd build
cmake -DENABLE_TESTING=ON \
      -DANTLR_JAR_LOCATION=/usr/local/lib/antlr-4.8-complete.jar \
      -DGRAMMAR_FILE=$(realpath ../grammars/ruby.json) \
      -DGRAMMAR_FILENAME=ruby ../
# Generated targets: libgrammarmutator-ruby.so, grammar_generator-ruby
make test
make test_memcheck  # if with Valgrind installed
```
