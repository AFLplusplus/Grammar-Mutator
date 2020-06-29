# Grammar-Mutator

Usage:

```bash
# A JSON parser
git clone https://github.com/h1994st/json-parser.git
cd json-parser
CC=afl-clang CXX=afl-clang++ ./configure
make
afl-clang examples/test_json.c -I. libjsonparser.a -lm -o test_json
cd ..

# Grammar-Mutator
git clone https://github.com/AFLplusplus/Grammar-Mutator.git
cd Grammar-Mutator
git checkout dev
export AFLPP_ROOT="/path/to/AFLplusplus"  # set the path
make src
cd examples/JSON
mkdir out
export AFL_CUSTOM_MUTATOR_ONLY=1
export AFL_CUSTOM_MUTATOR_LIBRARY=$(realpath ../../src/libjsonmutator.so)
afl-fuzz -i in -o out -- ../../../json-parser/test_json @@
```
