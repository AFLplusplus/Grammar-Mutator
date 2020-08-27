Building Fuzzing Targets
===

In this document, we show how to compile two fuzzing targets:

- `json-parser`
- `mruby`

## `json-parser`

```bash
git clone https://github.com/h1994st/json-parser.git
cd json-parser
CC=afl-clang CXX=afl-clang++ ./configure
make
afl-clang examples/test_json.c -I. libjsonparser.a -lm -o test_json
# Target: `test_json`
```

## `mruby`

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
