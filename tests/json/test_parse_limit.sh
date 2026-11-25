#!/bin/sh
# Check that the parse time limit of a JSON grammar build stops a slow parse
# (tests/json/parse_limit_check.c).
# Usage: tests/json/test_parse_limit.sh <grammar name>
# Run it from the repository root after a build of
# tests/json/ambiguous_list.json. Its grammar name is "ambiguous". Set
# BUILD_DIR for CMake.
set -eu

if [ $# -ne 1 ]; then
  echo "Usage: $0 <grammar name>" >&2
  exit 2
fi
name=$1

lib_dir=${BUILD_DIR:-.}/src
lib=$lib_dir/libgrammarmutator-$name.so
if [ ! -f "$lib" ] && [ -f "$lib_dir/libgrammarmutator-$name.dylib" ]; then
  lib=$lib_dir/libgrammarmutator-$name.dylib
fi
if [ ! -f "$lib" ]; then
  echo "[-] Cannot find $lib. Build the grammar first." >&2
  exit 2
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

${CC:-cc} -I include -I third_party/rxi_map -o "$work/parse_limit_check" \
  tests/json/parse_limit_check.c "$lib" -Wl,-rpath,"$(cd "$lib_dir" && pwd)"

# Keep the messages of the mutator in a file
status=0
"$work/parse_limit_check" "$work/out" 2>"$work/stderr.txt" || status=$?
cat "$work/stderr.txt" >&2

# The mutator warns once per run when the limit stops a parse
count=$(grep -c "a parse passed the time limit" "$work/stderr.txt" || true)
if [ "$status" -eq 0 ] && [ "$count" -ne 1 ]; then
  echo "[-] the stopped-parse warning appears $count times, not once" >&2
  status=1
fi
exit $status
