#!/bin/sh
# Check the cache mode of the benchmark program of a grammar.
# Usage: tests/benchmark/check_cache.sh <grammar name> <seed dir>
# Run it from the repository root after a build. Set BUILD_DIR for CMake.
set -eu

if [ $# -ne 2 ]; then
  echo "Usage: $0 <grammar name> <seed dir>" >&2
  exit 2
fi
name=$1
seeds=$2

unset PARSE_TIME_BUDGET_MS

bench=${BUILD_DIR:-.}/src/benchmark/benchmark-$name
if [ ! -x "$bench" ]; then
  echo "[-] Cannot find $bench. Build the grammar first." >&2
  exit 2
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

fail() {
  echo "[-] $1" >&2
  exit 1
}

status=0
"$bench" cache -m 200 -i 50 "$seeds" > "$work/cache.txt" 2> "$work/cache.err" ||
  status=$?
[ "$status" -eq 0 ] || fail "cache: exit $status"

grep -Eq '^benchmark version=[^ ]+ grammar='"$name"' system=[^ ]+ machine=[^ ]+ optimized=[01] mode=cache seed=0 mutants=200 interval=50 parse_budget_ms=[0-9]+ seeds=[0-9]+$' \
  "$work/cache.txt" || fail "cache: bad header"
[ "$(grep -c '^cache phase=seeds ' "$work/cache.txt")" -eq 1 ] ||
  fail "cache: expected one seed record"
[ "$(grep -c '^cache phase=mutants ' "$work/cache.txt")" -eq 4 ] ||
  fail "cache: expected four mutant records"
tail -n 1 "$work/cache.txt" | grep -Eq '^elapsed wall_ms=[0-9]+\.[0-9]$' ||
  fail "cache: the last record must be elapsed"

# Check the format and the order of the records. The first mutant record is
# 50 parses after the seed record, as the later ones are.
num='[0-9]+'
tm='[0-9]+\.[0-9]'
grep '^cache ' "$work/cache.txt" > "$work/records.txt"
prev_parses=
prev_parser=0
prev_lexer=0
prev_rss=0
last_parser=0
last_lexer=0
while IFS= read -r line; do
  echo "$line" | grep -Eq "^cache phase=(seeds|mutants) parses=$num parser_states=$num lexer_states=$num heap_kb=(-1|$num) rss_kb=$num us_per_parse=$tm trees=$num not_utf8=$num stopped=$num\$" ||
    fail "cache: bad record: $line"
  field() {
    value=${line#*" $1="}
    echo "${value%% *}"
  }
  parses=$(field parses)
  parser=$(field parser_states)
  lexer=$(field lexer_states)
  rss=$(field rss_kb)
  if [ -n "$prev_parses" ]; then
    [ "$parses" -eq $((prev_parses + 50)) ] ||
      fail "cache: parses must grow by 50: $line"
  fi
  [ "$parser" -ge "$prev_parser" ] || fail "cache: parser states went down"
  [ "$lexer" -ge "$prev_lexer" ] || fail "cache: lexer states went down"
  [ "$rss" -ge "$prev_rss" ] || fail "cache: rss_kb went down"
  prev_parser=$parser
  prev_lexer=$lexer
  prev_rss=$rss
  last_parser=$parser
  last_lexer=$lexer
  prev_parses=$parses
done < "$work/records.txt"
[ $((last_parser + last_lexer)) -gt 0 ] ||
  fail "cache: the cache has no state at the end"
echo "[+] cache prints the records of $name"
