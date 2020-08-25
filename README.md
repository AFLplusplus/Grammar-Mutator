# Grammar-Mutator

![Grammar Mutator CI](https://github.com/AFLplusplus/Grammar-Mutator/workflows/Grammar%20Mutator%20CI/badge.svg)

## Get started

### Compile

#### CMake

```bash
# Grammar-Mutator
git clone https://github.com/AFLplusplus/Grammar-Mutator.git
cd Grammar-Mutator
git checkout dev

# Download ANTLR4.8 jar file
wget https://www.antlr.org/download/antlr-4.8-complete.jar

# Create CMake working directory
mkdir build
cd build

# Default grammar: JSON (./grammars/json_grammar.json)
cmake -DENABLE_TESTING=ON -DANTLR_JAR_LOCATION=$(realpath ../antlr-4.8-complete.jar) ../
make
make test
make test_memcheck

# Make sure afl-fuzz/afl-clang/afl-clang++ are installed
# The last test case will run afl-fuzz with the grammar mutator on json-parser

# Grammar: ./grammars/ruby_grammar.json
cmake -DENABLE_TESTING=ON -DANTLR_JAR_LOCATION=$(realpath ../antlr-4.8-complete.jar) -DGRAMMAR_FILE=$(realpath ../grammars/ruby_grammar.json) ../
make
make test
make test_memcheck
```

#### Makefile

```bash
git clone https://github.com/AFLplusplus/Grammar-Mutator.git
cd Grammar-Mutator
git checkout dev

# Download ANTLR4.8 jar file
wget https://www.antlr.org/download/antlr-4.8-complete.jar

# Default grammar: JSON (./grammars/json_grammar.json)
make ENABLE_TESTING=1 ANTLR_JAR_LOCATION=./antlr-4.8-complete.jar
make test
make test_memcheck

# Grammar: ./grammars/ruby_grammar.json
make ENABLE_TESTING=1 ANTLR_JAR_LOCATION=./antlr-4.8-complete.jar GRAMMAR_FILE=grammars/ruby_grammar.json
make test
make test_memcheck
```

### Generate fuzzing corpus

`grammar_generator` can be used to generate input fuzzing seeds and corresponding tree files:

```bash
./grammar_generator <seed> <max_num> <max_len> <output_dir>

# e.g.
./grammar_generator 123 100 1000 /tmp/seeds
```

### Fuzz targets

#### `json-parser`

```bash
git clone https://github.com/h1994st/json-parser.git
cd json-parser
CC=afl-clang CXX=afl-clang++ ./configure
make
afl-clang examples/test_json.c -I. libjsonparser.a -lm -o test_json
# Target: `test_json`
```

#### `mruby`

Reference: <https://github.com/putsi/afl-mruby>

```bash
git clone https://github.com/mruby/mruby.git
cd mruby
mv build_config.rb build_config.rb.bak
wget https://github.com/putsi/afl-mruby/raw/master/build_config.rb
cd mrbgems/mruby-bin-mruby/tools/mruby
mv mruby.c mruby.c.bak
wget https://github.com/putsi/afl-mruby/raw/master/stub.c
mv stub.c mruby.c
cd ../../../..
sudo apt install ruby-full
./minirake
# Target: ./bin/mruby
```
