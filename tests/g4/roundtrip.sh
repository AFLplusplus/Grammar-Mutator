#!/bin/sh
# Check that the trees of grammar_parser-<name> give the seed bytes again.
# Usage: tests/g4/roundtrip.sh <grammar name> <seeds dir>
# Run it from the repository root after a build. Set BUILD_DIR for CMake.
set -eu

if [ $# -ne 2 ]; then
  echo "Usage: $0 <grammar name> <seeds dir>" >&2
  exit 2
fi
name=$1
seeds=$2

if [ -n "${BUILD_DIR:-}" ]; then
  bin_dir=$BUILD_DIR/src
else
  bin_dir=.
  BUILD_DIR=.
fi
parser=$bin_dir/grammar_parser-$name
lib=$BUILD_DIR/src/libgrammarmutator-$name.so

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir "$work/trees"

"$parser" "$seeds" "$work/trees" | tee "$work/summary"
if ! grep -q 'no match 0$' "$work/summary"; then
  echo "[-] Some seeds have no match" >&2
  exit 1
fi

cc -I include -o "$work/tree_check" tests/g4/tree_check.c "$lib" \
  -Wl,-rpath,"$(cd "$(dirname "$lib")" && pwd)"
"$work/tree_check" "$seeds" "$work/trees"
