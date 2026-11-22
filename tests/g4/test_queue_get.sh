#!/bin/sh
# Check that the mutator library of a .g4 build writes tree files from
# afl_custom_queue_get (tests/g4/queue_get_check.c).
# Usage: tests/g4/test_queue_get.sh <grammar name>
# Run it from the repository root after a build of a .g4 grammar. Set
# BUILD_DIR for CMake. The grammar must accept "[1, [2]]" and "[3]", and
# reject "{". The JSON and the JavaScript grammars do.
set -eu

if [ $# -ne 1 ]; then
  echo "Usage: $0 <grammar name>" >&2
  exit 2
fi
name=$1

lib_dir=${BUILD_DIR:-.}/src
lib=$lib_dir/libgrammarmutator-$name.so
# CMake on macOS makes a .dylib file
if [ ! -f "$lib" ] && [ -f "$lib_dir/libgrammarmutator-$name.dylib" ]; then
  lib=$lib_dir/libgrammarmutator-$name.dylib
fi
if [ ! -f "$lib" ]; then
  echo "[-] Cannot find $lib. Build the grammar first." >&2
  exit 2
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

${CC:-cc} -I include -I third_party/rxi_map -o "$work/queue_get_check" \
  tests/g4/queue_get_check.c "$lib" -Wl,-rpath,"$(cd "$lib_dir" && pwd)"
"$work/queue_get_check" "$work/out"
