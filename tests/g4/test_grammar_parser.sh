#!/bin/sh
# Check the command line behavior of grammar_parser-<name>.
# Usage: tests/g4/test_grammar_parser.sh <grammar name> [<test name>...]
# Run it from the repository root after a build of a .g4 grammar. Set
# BUILD_DIR for CMake. The grammar must accept JSON arrays of numbers, for
# example "[1, [2]]". The JSON and the JavaScript grammars both do.
# The script needs python3 to read the tree files (grammars/tree_file.py).
set -eu

if [ $# -lt 1 ]; then
  echo "Usage: $0 <grammar name> [<test name>...]" >&2
  exit 2
fi
name=$1
shift

if [ -n "${BUILD_DIR:-}" ]; then
  parser=$BUILD_DIR/src/grammar_parser-$name
else
  parser=./grammar_parser-$name
fi
if [ ! -x "$parser" ]; then
  echo "[-] Cannot find $parser. Build the grammar first." >&2
  exit 2
fi
case $parser in
  /*) ;;
  *) parser=$(pwd)/$parser ;;
esac
root=$(pwd)

work=$(mktemp -d)
trap 'chmod -R u+rwx "$work" 2>/dev/null; rm -rf "$work"' EXIT

failed=0
count=0

# Report a failure of the current test
fail() {
  echo "    $*"
  : > "$dir/.failed"
}

# Run the parser with the arguments. Keep the exit code in $code, the standard
# output in $dir/stdout, and the standard error in $dir/stderr.
run() {
  code=0
  "$parser" "$@" > "$dir/stdout" 2> "$dir/stderr" || code=$?
}

expect_code() {
  if [ "$code" -ne "$1" ]; then
    fail "exit code $code, expected $1"
    sed 's/^/      /' "$dir/stderr"
  fi
}

# expect_file FILE TEXT: FILE has exactly TEXT (with no newline at the end)
expect_file() {
  printf '%s' "$2" > "$dir/expected"
  if ! cmp -s "$1" "$dir/expected"; then
    fail "$1 is not as expected:"
    diff "$dir/expected" "$1" | sed 's/^/      /' || true
  fi
}

expect_in_file() {
  if ! grep -qF -- "$2" "$1"; then
    fail "$1 does not have \"$2\":"
    sed 's/^/      /' "$1"
  fi
}

# The leaves of a tree file, joined
tree_text() {
  python3 -c 'import sys
sys.path.insert(0, sys.argv[1])
from tree_file import deserialize, text
sys.stdout.write(text(deserialize(open(sys.argv[2], "rb").read())))' "$root/grammars" "$1"
}

expect_tree() {
  if [ ! -f "$1" ]; then
    fail "no tree file $1"
    return
  fi
  tree_text "$1" > "$dir/tree_text"
  expect_file "$dir/tree_text" "$2"
}

# Write an input with more than 200,000 rule levels: nested arrays for JSON,
# nested parentheses for JavaScript
deep_input() {
  case $name in
    javascript) open='(' close=')' ;;
    *) open='[' close=']' ;;
  esac
  awk -v n=110000 -v o="$open" -v c="$close" \
    'BEGIN { for (i = 0; i < n; i++) printf "%s", o; printf "1"; for (i = 0; i < n; i++) printf "%s", c }'
}

# The parser is a program, not a script
test_is_a_program() {
  if [ "$(head -c 2 "$parser")" = "#!" ]; then
    fail "$parser is a script"
  fi
}

test_writes_trees_and_reports_skips() {
  mkdir -p seeds/sub
  printf '[1, [2]]' > seeds/a.json
  printf '[1, 2]\n' > "seeds/sub/b c.json"
  printf '{' > seeds/bad.json
  printf '[]' > seeds/sub/a.json
  printf '\377\376' > seeds/x.bin
  # AFL++ changes each space in a queue name to "_", so "b c.json" and
  # "b_c.json" have the same tree name.
  printf '[3]' > seeds/sub/b_c.json
  run seeds out/trees
  expect_code 0
  ls out/trees > list
  expect_file list "a.json
b_c.json
"
  expect_tree out/trees/a.json '[1, [2]]'
  expect_tree out/trees/b_c.json '[1, 2]
'
  expect_file stderr "[!] bad.json: syntax error
[!] sub/a.json: duplicate name
[!] sub/b_c.json: duplicate name
[!] x.bin: not UTF-8
"
  expect_file stdout "[+] Parsed 2 of 6 files; skipped: not UTF-8 1, syntax error 1, parser limit 0, duplicate name 2, no match 0
"
}

test_space_gives_underscore() {
  mkdir seeds
  printf '[1]' > "seeds/a b c.json"
  run seeds trees
  expect_code 0
  expect_tree "trees/a_b_c.json" '[1]'
}

test_reads_subfolders() {
  mkdir -p seeds/one/two
  printf '[1]' > seeds/one/two/deep.json
  run seeds trees
  expect_code 0
  expect_tree trees/deep.json '[1]'
  expect_file stdout "[+] Parsed 1 of 1 files; skipped: not UTF-8 0, syntax error 0, parser limit 0, duplicate name 0, no match 0
"
}

test_reports_parser_limit() {
  mkdir seeds
  deep_input > seeds/deep.json
  run seeds trees
  expect_code 0
  expect_file stderr "[!] deep.json: parser limit
"
  expect_in_file stdout "parser limit 1,"
}

test_jobs_give_the_same_output() {
  mkdir -p seeds/sub
  for i in 00 01 02 03 04 05 06 07 08 09 10 11; do
    printf '[1%s, [1]]' "$i" > "seeds/$i.json"
  done
  printf '{' > seeds/bad.json
  printf '\377' > seeds/x.bin
  printf '[]' > seeds/sub/03.json
  for jobs in 1 4; do
    mkdir "out$jobs"
    run -j "$jobs" seeds "out$jobs/trees"
    echo "$code" > "out$jobs/code"
    mv stdout stderr "out$jobs/"
  done
  if ! diff -r out1 out4 > differences; then
    fail "-j 1 and -j 4 give different output:"
    sed 's/^/      /' differences
  fi
  find out1/trees -type f | wc -l | tr -d ' ' > tree_count
  expect_file tree_count "12
"
  expect_in_file out1/stdout "not UTF-8 1, syntax error 1, parser limit 0, duplicate name 1,"
}

# expect_usage_error ARGS...: the parser exits 2 with a message
expect_usage_error() {
  run "$@"
  if [ "$code" -ne 2 ]; then
    fail "arguments \"$*\": exit code $code, expected 2"
  fi
  expect_in_file stderr "[-] "
}

test_bad_options_exit_2() {
  mkdir seeds
  expect_usage_error -j 0 seeds trees
  expect_usage_error -j x seeds trees
  expect_usage_error -j
  expect_usage_error -x seeds trees
  expect_usage_error seeds
  expect_usage_error seeds trees more
  if [ -e trees ]; then
    fail "a bad command line made the folder trees"
  fi
}

test_read_error_exits_2() {
  if [ "$(id -u)" -eq 0 ]; then
    echo "    skipped: root can read any file"
    return
  fi
  mkdir seeds
  for f in a b c; do printf '[]' > "seeds/$f.json"; done
  chmod 0 seeds/b.json
  run -j 2 seeds trees
  chmod 644 seeds/b.json
  expect_code 2
  expect_in_file stderr "[-] Cannot read b.json: "
}

test_removes_the_old_tree_of_a_skipped_file() {
  mkdir seeds
  printf '[]' > seeds/a.json
  run seeds trees
  expect_code 0
  if [ ! -f trees/a.json ]; then
    fail "no tree for a.json"
  fi
  printf '[' > seeds/a.json
  run seeds trees
  expect_code 0
  if [ -e trees/a.json ]; then
    fail "the old tree of a.json stays"
  fi
}

test_keeps_a_file_that_is_not_a_tree() {
  mkdir seeds trees
  printf '[' > seeds/a.json
  printf 'notes' > trees/a.json
  run seeds trees
  expect_code 0
  expect_file trees/a.json 'notes'
  expect_file stderr "[!] a.json: syntax error
[!] trees/a.json is not a tree. It stays.
"
}

test_duplicate_names_follow_the_file_system() {
  # A file system that ignores case (macOS) gives A.json and a.json one tree
  # file. A file system with case (Linux) gives two.
  mkdir -p seeds/sub
  printf '[]' > seeds/A.json
  printf '[1]' > seeds/sub/a.json
  : > probe
  run seeds trees
  expect_code 0
  expect_tree trees/A.json '[]'
  if [ -e PROBE ]; then
    expect_file stderr "[!] sub/a.json: duplicate name
"
  else
    expect_file stderr ""
    expect_tree trees/a.json '[1]'
  fi
}

test_refuses_trees_in_the_seeds_folder() {
  mkdir seeds
  printf '[]' > seeds/a.json
  ln -s seeds link
  set -- seeds seeds/trees seeds/../seeds link link/trees "$dir/seeds/x/y"
  : > probe
  if [ -e PROBE ]; then
    # A file system that ignores case
    set -- "$@" SEEDS SEEDS/trees
  fi
  for trees in "$@"; do
    run seeds "$trees"
    if [ "$code" -ne 2 ]; then
      fail "TREES_DIR $trees: exit code $code, expected 2"
    fi
    expect_file stderr "[-] TREES_DIR must not be SEEDS_DIR or a folder in SEEDS_DIR
"
  done
  expect_file seeds/a.json '[]'
  ls seeds > list
  expect_file list "a.json
"
  # A sibling folder through ".." is not in SEEDS_DIR.
  run seeds seeds/../other
  expect_code 0
  expect_file stderr ""
  expect_tree other/a.json '[]'
}

test_prints_undecodable_file_names() {
  if [ "$(uname -s)" = Darwin ]; then
    echo "    skipped: macOS file names must be UTF-8"
    return
  fi
  mkdir seeds
  if ! printf '[' > "seeds/$(printf 'bad\377.json')" 2>/dev/null; then
    echo "    skipped: the file system needs UTF-8 file names"
    return
  fi
  printf '[1]' > "seeds/$(printf 'caf\303\251.json')"
  run seeds trees
  expect_code 0
  printf '[!] bad\\xff.json: syntax error\n' > expected_err
  if ! cmp -s stderr expected_err; then
    fail "the file names are not as expected:"
    od -c stderr | sed 's/^/      /'
  fi
  expect_tree "trees/$(printf 'caf\303\251.json')" '[1]'
}

test_seeds_dir_that_is_not_a_folder_exits_2() {
  printf '[]' > file
  run file trees
  expect_code 2
  expect_file stderr "[-] Not a folder: file
"
  run missing trees
  expect_code 2
  expect_file stderr "[-] Not a folder: missing
"
}

all_tests="
test_is_a_program
test_writes_trees_and_reports_skips
test_space_gives_underscore
test_reads_subfolders
test_reports_parser_limit
test_jobs_give_the_same_output
test_bad_options_exit_2
test_read_error_exits_2
test_removes_the_old_tree_of_a_skipped_file
test_keeps_a_file_that_is_not_a_tree
test_duplicate_names_follow_the_file_system
test_refuses_trees_in_the_seeds_folder
test_prints_undecodable_file_names
test_seeds_dir_that_is_not_a_folder_exits_2
"
if [ $# -gt 0 ]; then
  all_tests=$*
fi

for t in $all_tests; do
  count=$((count + 1))
  dir=$work/$t
  mkdir "$dir"
  status=0
  (cd "$dir" && "$t") > "$work/log" 2>&1 || status=$?
  if [ "$status" -eq 0 ] && [ ! -e "$dir/.failed" ]; then
    echo "[ OK ] $t"
    cat "$work/log"
  else
    echo "[FAIL] $t"
    cat "$work/log"
    failed=$((failed + 1))
  fi
done

echo "$count tests, $failed failed"
[ "$failed" -eq 0 ]
