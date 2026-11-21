#!/bin/sh
# Check that the trees of grammar_parser-<name> give the seed bytes again.
# Usage: tests/g4/roundtrip.sh <grammar name> <seeds dir>
# Run it from the repository root after a build. Set BUILD_DIR for CMake.
# Every seed must give a tree. Set ALLOW_SKIPS=1 to allow skipped seeds;
# then only "no match 0" is required.
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

status=0
"$parser" "$seeds" "$work/trees" > "$work/summary" || status=$?
cat "$work/summary"
if [ "$status" -ne 0 ]; then
  echo "[-] grammar_parser-$name failed with exit code $status" >&2
  exit 1
fi
if ! grep -q 'no match 0$' "$work/summary"; then
  echo "[-] Some seeds have no match" >&2
  exit 1
fi
if [ -z "${ALLOW_SKIPS:-}" ]; then
  if ! grep -q 'not UTF-8 0, syntax error 0, parser limit 0, duplicate name 0, no match 0$' "$work/summary"; then
    echo "[-] The parser skipped some seeds" >&2
    exit 1
  fi
  seed_count=$(find "$seeds" -type f | wc -l)
  tree_count=$(find "$work/trees" -type f | wc -l)
  if [ "$seed_count" -ne "$tree_count" ]; then
    echo "[-] $seed_count seeds but $tree_count trees" >&2
    exit 1
  fi
fi

cc -I include -o "$work/tree_check" tests/g4/tree_check.c "$lib" \
  -Wl,-rpath,"$(cd "$(dirname "$lib")" && pwd)"
"$work/tree_check" "$seeds" "$work/trees"
