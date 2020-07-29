# Grammar-Mutator

![Grammar Mutator CI](https://github.com/AFLplusplus/Grammar-Mutator/workflows/Grammar%20Mutator%20CI/badge.svg)

## Get started

### CMake

```bash
# Grammar-Mutator
git clone https://github.com/AFLplusplus/Grammar-Mutator.git
cd Grammar-Mutator
git checkout dev
mkdir build
cd build
cmake -DENABLE_TESTING=ON ../
make
make test
make test_memcheck

# Make sure afl-fuzz/afl-clang/afl-clang++ are installed
# The last test case will run afl-fuzz with the grammar mutator on json-parser
```

# Makefile

```bash
git clone https://github.com/AFLplusplus/Grammar-Mutator.git
cd Grammar-Mutator
git checkout dev
make ENABLE_TESTING=1
make test
make test_memcheck
```
