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

"$gen" 20 1000 "$work/seeds" "$work/trees" 1 > /dev/null

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

# run mode: a subfolder and a dot file are skipped
mkdir "$work/seeds/sub"
echo x > "$work/seeds/sub/a"
echo x > "$work/seeds/.hidden"
find "$work/seeds" -type f -exec cksum {} + | sort > "$work/before.txt"
"$bench" run -e 3 -n 50 -t 20 "$work/seeds" > "$work/run.txt" ||
  fail "run: nonzero exit"
grep -Eq '^benchmark version=[^ ]+ grammar='"$name"' system=[^ ]+ machine=[^ ]+ optimized=[01] mode=run seed=0 entries=3 steps=50 trims=20 parse_budget_ms=0 seeds=20 trees=20$' \
  "$work/run.txt" || fail "run: bad header"
[ "$(grep -c '^seed ' "$work/run.txt")" -eq 20 ] || fail "run: seed records"
[ "$(grep -c '^entry ' "$work/run.txt")" -eq 3 ] || fail "run: entry records"
grep -q '^store seeds=20 chunks=[1-9][0-9]* fill_ms=' "$work/run.txt" ||
  fail "run: store record"
for s in rules random splice; do
  [ "$(grep -c "^stage .* stage=$s steps=" "$work/run.txt")" -eq 3 ] ||
    fail "run: stage $s"
done
[ "$(grep -c '^trim ' "$work/run.txt")" -eq 3 ] || fail "run: trim records"
grep -q '^peak rss_kb=[1-9]' "$work/run.txt" || fail "run: peak record"
find "$work/seeds" -type f -exec cksum {} + | sort | cmp - "$work/before.txt" ||
  fail "run: the seed folder changed"
echo "[+] run prints the records and leaves the seeds alone"

# run mode: -e larger than the trees measures each tree once
[ "$("$bench" run -e 50 -n 5 -t 5 "$work/seeds" | grep -c '^entry ')" -eq 20 ] ||
  fail "run: -e 50 must give each tree once"
echo "[+] run measures each tree once if -e is large"

# run mode: bad input
for args in "run -e 0 $work/seeds" "run -n abc $work/seeds" "run" \
  "nosuchmode $work/seeds"; do
  status=0
  # shellcheck disable=SC2086
  "$bench" $args > /dev/null 2>&1 || status=$?
  [ "$status" -eq 2 ] || fail "'$args': expected exit 2, got $status"
done
mkdir "$work/empty"
status=0
"$bench" run "$work/empty" > /dev/null 2>&1 || status=$?
[ "$status" -eq 1 ] || fail "run on an empty folder: expected exit 1"
status=0
"$bench" run "$work/no-such-dir" > /dev/null 2>&1 || status=$?
[ "$status" -eq 1 ] || fail "run on a missing folder: expected exit 1"
[ -z "$(ls -A "$work/tmp")" ] || fail "run left a work folder behind"
echo "[+] run rejects bad input and leaves no work folder"
