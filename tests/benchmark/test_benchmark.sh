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

# Run a command. Keep its output in the file $1 and its error text in $1.err.
# The exit status goes to the variable "status".
capture() {
  capture_out=$1
  shift
  status=0
  "$@" > "$capture_out" 2> "$capture_out.err" || status=$?
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
# each record: bytes is the size of the file, nodes is more than 0
while IFS= read -r line; do
  echo "$line" | grep -Eq '^parse file=[^ ]+ bytes=[0-9]+ nodes=[0-9]+ us=[0-9]+\.[0-9] result=ok$' ||
    fail "parse: bad record format: $line"
  file=${line#parse file=}
  file=${file%% *}
  bytes=${line#*' bytes='}
  bytes=${bytes%% *}
  nodes=${line#*' nodes='}
  nodes=${nodes%% *}
  [ "$bytes" -eq "$(wc -c < "$file")" ] || fail "parse: wrong bytes: $line"
  [ "$nodes" -gt 0 ] || fail "parse: no nodes: $line"
done < "$work/parse.txt"
echo "[+] parse prints one ok record for each seed"

# parse mode: an unreadable file gives result=error and exit 1
capture "$work/parse2.txt" "$bench" parse "$work/no-such-file" \
  "$work/seeds/$(ls "$work/seeds" | head -1)"
[ "$status" -eq 1 ] || fail "parse: expected exit 1, got $status"
grep -q ' result=error$' "$work/parse2.txt" || fail "parse: no result=error"
grep -q "no-such-file: " "$work/parse2.txt.err" ||
  fail "parse: no reason on stderr for the unreadable file"
grep -q ' result=ok$' "$work/parse2.txt" || fail "parse: other file not printed"
# the reason is the error of the open call, also if stdout is /dev/null (on
# macOS the first output of stdio sets errno to ENOTTY)
status=0
"$bench" parse "$work/no-such-file" > /dev/null 2> "$work/errno.txt" || status=$?
[ "$status" -eq 1 ] || fail "parse to /dev/null: expected exit 1, got $status"
grep -q 'No such file or directory' "$work/errno.txt" ||
  fail "parse: wrong reason: $(cat "$work/errno.txt")"
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
capture "$work/run.txt" "$bench" run -e 3 -n 50 -t 20 "$work/seeds"
[ "$status" -eq 0 ] || fail "run: exit $status"
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
# field order and number formats of the records
num='[0-9]+'
tm='[0-9]+\.[0-9]'
grep '^seed ' "$work/run.txt" |
  grep -Evq "^seed name=[^ ]+ bytes=$num parse_us=$tm tree=[01]\$" &&
  fail "run: bad seed record"
grep '^entry ' "$work/run.txt" |
  grep -Evq "^entry name=[^ ]+ bytes=$num nodes=$num rules_pairs=$num repick_us=$tm count_us=$tm\$" &&
  fail "run: bad entry record"
grep '^stage ' "$work/run.txt" |
  grep -Evq "^stage entry=[^ ]+ stage=(rules|random|recursive|splice) steps=$num us_per_output=$tm avg_bytes=$num dup_pct=$tm same_pct=$tm\$" &&
  fail "run: bad stage record"
grep '^trim ' "$work/run.txt" |
  grep -Evq "^trim entry=[^ ]+ steps=$num us_per_step=$tm\$" &&
  fail "run: bad trim record"
find "$work/seeds" -type f -exec cksum {} + | sort | cmp - "$work/before.txt" ||
  fail "run: the seed folder changed"
echo "[+] run prints the records and leaves the seeds alone"

# run mode: the entries are the smallest, the middle, and the largest seed by
# bytes (ties by name), as the rank formula (2 * i * (n - 1) + (e - 1)) /
# (2 * (e - 1)) says. The names are in ascending order of size.
for file in "$work"/seeds/*; do
  [ -f "$file" ] && echo "$(wc -c < "$file" | tr -d ' ') ${file##*/}"
done | LC_ALL=C sort -k1,1n -k2,2 > "$work/sizes.txt"
awk 'NR == 1 || NR == 11 || NR == 20 { print $2 }' "$work/sizes.txt" \
  > "$work/expected-entries.txt"
sed -n 's/^entry name=\([^ ]*\) .*/\1/p' "$work/run.txt" \
  > "$work/actual-entries.txt"
cmp "$work/expected-entries.txt" "$work/actual-entries.txt" ||
  fail "run: -e 3 must pick the smallest, middle, and largest seed"
echo "[+] run picks the smallest, middle, and largest seed"

# an option value can be attached to its flag, and -- ends the options
capture "$work/attached.txt" "$bench" run -e3 -n50 -t20 "$work/seeds"
[ "$status" -eq 0 ] || fail "run -e3 -n50 -t20: exit $status"
[ "$(grep '^benchmark ' "$work/attached.txt")" = \
  "$(grep '^benchmark ' "$work/run.txt")" ] ||
  fail "run: attached values must give the same header"
capture "$work/dashdash.txt" "$bench" run -e 1 -n 5 -t 5 -- "$work/seeds"
[ "$status" -eq 0 ] || fail "run -- DIR: exit $status"
[ "$(grep -c '^entry ' "$work/dashdash.txt")" -eq 1 ] ||
  fail "run -- DIR: expected one entry"
echo "[+] run accepts attached option values and --"

# a signal stops the program, removes the work folder, and keeps the signal
# as the exit status. The program makes the trees folder after it starts the
# handlers, so the test waits for it.
# Wait until the run in the TMPDIR $1 made its trees folder. Stop all started
# background runs and fail if that takes more than 10 s.
bg_pids=
wait_for_trees() {
  tries=0
  while ! ls -d "$1"/benchmark.*/trees > /dev/null 2>&1; do
    tries=$((tries + 1))
    if [ "$tries" -gt 100 ]; then
      # shellcheck disable=SC2086
      kill $bg_pids 2> /dev/null || true
      fail "the run did not start in 10 s ($1)"
    fi
    sleep 0.1
  done
}

mkdir "$work/tmp-int" "$work/tmp-term" "$work/tmp-hup"
for sig in int term hup; do
  TMPDIR="$work/tmp-$sig" "$bench" run -n 1000000 -t 1 -e 1 "$work/seeds" \
    > /dev/null 2> "$work/$sig.err" &
  eval "pid_$sig=\$!"
  bg_pids="$bg_pids $!"
done
for sig in int term hup; do
  wait_for_trees "$work/tmp-$sig"
done
kill -INT "$pid_int"
kill -TERM "$pid_term"
kill -HUP "$pid_hup"
status_int=0
status_term=0
status_hup=0
{
  wait "$pid_int" || status_int=$?
  wait "$pid_term" || status_term=$?
  wait "$pid_hup" || status_hup=$?
} 2> /dev/null
for sig in int term hup; do
  eval "st=\$status_$sig"
  [ "$st" -gt 128 ] || fail "$sig: expected a status above 128, got $st"
  [ -s "$work/$sig.err" ] || fail "$sig: no message on stderr"
  [ -z "$(ls -A "$work/tmp-$sig")" ] || fail "$sig left a work folder behind"
done
echo "[+] SIGINT, SIGTERM, and SIGHUP remove the work folder"

# a run that starts with SIGHUP ignored (as under nohup) goes on after a
# hang-up, and a later SIGTERM stops it
mkdir "$work/tmp-nohup"
(
  trap '' HUP
  TMPDIR="$work/tmp-nohup" exec "$bench" run -n 1000000 -t 1 -e 1 "$work/seeds" \
    > /dev/null 2> "$work/nohup.err"
) &
pid_nohup=$!
bg_pids=$pid_nohup
wait_for_trees "$work/tmp-nohup"
kill -HUP "$pid_nohup"
sleep 0.3
if ! kill -0 "$pid_nohup" 2> /dev/null; then
  fail "a run with SIGHUP ignored stopped on SIGHUP"
fi
kill -TERM "$pid_nohup"
{ wait "$pid_nohup" || true; } 2> /dev/null
[ -z "$(ls -A "$work/tmp-nohup")" ] || fail "the nohup run left a work folder"
echo "[+] a run with SIGHUP ignored goes on after SIGHUP"

# a failed write to stdout gives exit 1 (a closed stdout gives EBADF)
status=0
"$bench" parse "$work"/seeds/* >&- 2> /dev/null || status=$?
[ "$status" -eq 1 ] || fail "parse with a closed stdout: expected exit 1, got $status"
mkdir "$work/tmp-closed"
status=0
TMPDIR="$work/tmp-closed" "$bench" run -e 1 -n 5 -t 5 "$work/seeds" >&- 2> /dev/null ||
  status=$?
[ "$status" -eq 1 ] || fail "run with a closed stdout: expected exit 1, got $status"
status=0
TMPDIR="$work/tmp-closed" "$bench" digest -n 5 -t 5 "$work/seeds" >&- 2> /dev/null ||
  status=$?
[ "$status" -eq 1 ] || fail "digest with a closed stdout: expected exit 1, got $status"
[ -z "$(ls -A "$work/tmp-closed")" ] || fail "a failed write left a work folder"
echo "[+] a failed write to stdout gives exit 1"

# a closed pipe (SIGPIPE) ends the run, with the same cleanup. The run prints
# a record after each entry, so a write comes after head has left. The run
# is short, so the test cannot hang.
mkdir "$work/tmp-pipe"
TMPDIR="$work/tmp-pipe" "$bench" run -e 20 -n 2000 -t 10 "$work/seeds" 2> /dev/null |
  head -1 > /dev/null
[ -z "$(ls -A "$work/tmp-pipe")" ] || fail "a closed pipe left a work folder"
echo "[+] a closed pipe removes the work folder"

# run mode: -e larger than the trees measures each tree once
capture "$work/many.txt" "$bench" run -e 50 -n 5 -t 5 "$work/seeds"
[ "$status" -eq 0 ] || fail "run -e 50: exit $status"
[ "$(grep -c '^entry ' "$work/many.txt")" -eq 20 ] ||
  fail "run: -e 50 must give each tree once"

# zero steps and zero trims
capture "$work/n0.txt" "$bench" run -e 1 -n 0 -t 5 "$work/seeds"
[ "$status" -eq 0 ] || fail "run -n 0: exit $status"
if grep -q '^stage ' "$work/n0.txt"; then fail "run -n 0: a stage record"; fi
[ "$(grep -c '^trim ' "$work/n0.txt")" -eq 1 ] || fail "run -n 0: no trim record"
capture "$work/t0.txt" "$bench" run -e 1 -n 5 -t 0 "$work/seeds"
[ "$status" -eq 0 ] || fail "run -t 0: exit $status"
grep -Eq '^trim entry=[^ ]+ steps=0 ' "$work/t0.txt" ||
  fail "run -t 0: expected trim steps=0"
capture "$work/d0.txt" "$bench" digest -n 0 -t 0 "$work/seeds"
[ "$status" -eq 0 ] || fail "digest -n 0 -t 0: exit $status"
# A NULL or empty last output cannot be made with ruby.json, so no test here
# runs the NULL branches of the digest and run modes.
echo "[+] run measures each tree once if -e is large"

# run mode: bad input
for args in "run -e 0 $work/seeds" "run -n abc $work/seeds" "run" \
  "nosuchmode $work/seeds"; do
  status=0
  # shellcheck disable=SC2086
  "$bench" $args > /dev/null 2> "$work/err.txt" || status=$?
  [ "$status" -eq 2 ] || fail "'$args': expected exit 2, got $status"
  grep -q '^Usage:' "$work/err.txt" || fail "'$args': no usage text"
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

# digest mode: the same input gives the same hash, in another work folder
rm -rf "$work/seeds/sub" "$work/seeds/.hidden"
capture "$work/d1.txt" "$bench" digest -n 50 -t 10 "$work/seeds"
[ "$status" -eq 0 ] || fail "digest: exit $status"
mkdir "$work/tmp2"
capture "$work/d2.txt" env TMPDIR="$work/tmp2" "$bench" digest -n 50 -t 10 "$work/seeds"
[ "$status" -eq 0 ] || fail "digest in another work folder: exit $status"
grep -Eq '^benchmark .* mode=digest seed=0 steps=50 trims=10 parse_budget_ms=0 seeds=20 trees=20$' \
  "$work/d1.txt" || fail "digest: bad header"
grep -Eq '^digest outputs=[1-9][0-9]* new_entries=20 trims=[1-9][0-9]* hash=[0-9a-f]{16}$' \
  "$work/d1.txt" || fail "digest: bad record"
d1=$(grep '^digest ' "$work/d1.txt")
[ "$d1" = "$(grep '^digest ' "$work/d2.txt")" ] ||
  fail "digest: the hash depends on the work folder"
# another random seed gives another hash
capture "$work/d3.txt" "$bench" digest -n 50 -t 10 -s 1 "$work/seeds"
[ "$status" -eq 0 ] || fail "digest -s 1: exit $status"
[ "$(grep '^digest ' "$work/d3.txt")" != "$d1" ] ||
  fail "digest: another random seed gives the same hash"
# a new entry never replaces a seed, also if a seed has the name of an entry.
# The names "9" and "zz9" sort last in the same way, so the content in the
# same order must give the same record.
mkdir "$work/shadow-a" "$work/shadow-b"
for file in "$work"/seeds/*; do
  base=${file##*/}
  if [ "$base" = 9 ]; then
    cp "$file" "$work/shadow-a/new_000000"
    cp "$file" "$work/shadow-b/zz9"
  else
    cp "$file" "$work/shadow-a/$base"
    cp "$file" "$work/shadow-b/$base"
  fi
done
capture "$work/sa.txt" "$bench" digest -n 50 -t 10 "$work/shadow-a"
[ "$status" -eq 0 ] || fail "digest shadow-a: exit $status"
capture "$work/sb.txt" "$bench" digest -n 50 -t 10 "$work/shadow-b"
[ "$status" -eq 0 ] || fail "digest shadow-b: exit $status"
[ "$(grep '^digest ' "$work/sa.txt")" = "$(grep '^digest ' "$work/sb.txt")" ] ||
  fail "digest: a seed named new_000000 changed the digest"
# -e is not a digest option
status=0
"$bench" digest -e 3 "$work/seeds" > /dev/null 2>&1 || status=$?
[ "$status" -eq 2 ] || fail "digest -e: expected exit 2, got $status"
[ -z "$(ls -A "$work/tmp")" ] && [ -z "$(ls -A "$work/tmp2")" ] ||
  fail "digest left a work folder behind"
echo "[+] digest gives a stable hash and leaves no work folder"

# parse time limit: an empty value is the same as unset; a value shows safely
PARSE_TIME_BUDGET_MS= "$bench" run -e 1 -n 5 -t 5 "$work/seeds" \
  > "$work/budget1.txt" 2> /dev/null || fail "empty budget: nonzero exit"
grep '^benchmark ' "$work/budget1.txt" | grep -q ' parse_budget_ms=0 ' ||
  fail "an empty PARSE_TIME_BUDGET_MS must give parse_budget_ms=0"
PARSE_TIME_BUDGET_MS="5 0" "$bench" run -e 1 -n 5 -t 5 "$work/seeds" \
  > "$work/budget2.txt" 2> /dev/null || fail "odd budget: nonzero exit"
grep '^benchmark ' "$work/budget2.txt" | grep -q ' parse_budget_ms=5?0 ' ||
  fail "a blank in PARSE_TIME_BUDGET_MS must print as '?'"
echo "[+] an empty PARSE_TIME_BUDGET_MS turns the limit off; blanks print as ?"

# seeds without a tree: a file that is not UTF-8 gives no tree
mkdir "$work/notree"
printf '\377' > "$work/notree/bad1"
printf '\377\376' > "$work/notree/bad2"
for mode in run digest; do
  status=0
  "$bench" $mode "$work/notree" > /dev/null 2> "$work/err.txt" || status=$?
  [ "$status" -eq 1 ] || fail "$mode without a tree: expected exit 1, got $status"
  [ -s "$work/err.txt" ] || fail "$mode without a tree: no message on stderr"
done
[ -z "$(ls -A "$work/tmp")" ] || fail "a run without a tree left a work folder"
echo "[+] run and digest reject seeds without a tree"

# digest: a seed without a tree changes the hash
mkdir "$work/mixed"
cp "$work"/seeds/* "$work/mixed/"
printf '\377' > "$work/mixed/zz-bad"
capture "$work/dm.txt" "$bench" digest -n 50 -t 10 "$work/mixed"
[ "$status" -eq 0 ] || fail "digest mixed: exit $status"
grep '^benchmark ' "$work/dm.txt" | grep -q ' seeds=21 trees=20$' ||
  fail "digest mixed: expected seeds=21 trees=20"
[ "$(grep '^digest ' "$work/dm.txt")" != "$d1" ] ||
  fail "digest: a seed without a tree must change the hash"
echo "[+] digest counts a seed without a tree"
