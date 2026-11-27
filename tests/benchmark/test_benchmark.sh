#!/bin/sh
# Check the benchmark program of a grammar (src/benchmark/benchmark.c).
# Usage: tests/benchmark/test_benchmark.sh <grammar name>
# Run it from the repository root after a build. Set BUILD_DIR for CMake.
set -eu

if [ $# -ne 1 ]; then
  echo "Usage: $0 <grammar name>" >&2
  exit 2
fi
name=$1

# Do not let the shell of the developer change the results
unset PARSE_TIME_BUDGET_MS RULES_MUTATION_STEPS RANDOM_MUTATION_STEPS \
  RANDOM_RECURSIVE_MUTATION_STEPS SPLICING_MUTATION_STEPS TRIMMING_STEPS

bin=${BUILD_DIR:-.}/src
bench=$bin/benchmark/benchmark-$name
gen=$bin/grammar_generator-$name
for prog in "$bench" "$gen"; do
  if [ ! -x "$prog" ]; then
    echo "[-] Cannot find $prog. Build the grammar first." >&2
    exit 2
  fi
done

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir "$work/tmp"
export TMPDIR="$work/tmp"

fail() {
  echo "[-] $1" >&2
  exit 1
}

"$gen" 20 100 "$work/seeds" "$work/trees" 1 > /dev/null

# parse mode: one record for each seed file, each one ok
"$bench" parse "$work"/seeds/* > "$work/parse.txt"
count=$(ls "$work/seeds" | wc -l)
[ "$(grep -c '^parse ' "$work/parse.txt")" -eq "$count" ] ||
  fail "parse: expected $count records"
if grep '^parse ' "$work/parse.txt" | grep -v ' result=ok$' > /dev/null; then
  fail "parse: a seed does not give result=ok"
fi
grep -Eq '^parse file=[^ ]+ bytes=[0-9]+ nodes=[0-9]+ us=[0-9]+\.[0-9] result=ok$' \
  "$work/parse.txt" || fail "parse: bad record format"
echo "[+] parse prints one ok record for each seed"

# parse mode: an unreadable file gives result=error and exit 1
status=0
"$bench" parse "$work/no-such-file" \
  "$work/seeds/$(ls "$work/seeds" | head -1)" > "$work/parse2.txt" || status=$?
[ "$status" -eq 1 ] || fail "parse: expected exit 1, got $status"
grep -q ' result=error$' "$work/parse2.txt" || fail "parse: no result=error"
grep -q ' result=ok$' "$work/parse2.txt" || fail "parse: other file not printed"
echo "[+] parse reports an unreadable file and goes on"

# usage errors
for args in "" "bogus" "parse" "run" "digest"; do
  status=0
  # shellcheck disable=SC2086
  "$bench" $args > /dev/null 2> "$work/err.txt" || status=$?
  [ "$status" -eq 2 ] || fail "'$args': expected exit 2, got $status"
  grep -q '^Usage:' "$work/err.txt" || fail "'$args': no usage text"
done
echo "[+] a missing or unknown mode gives the usage text and exit 2"
