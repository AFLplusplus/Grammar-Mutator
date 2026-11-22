#!/bin/sh
# Check grammar_parser-javascript on the JerryScript v3.0.0 test suite.
# Usage: tests/g4/check_jerryscript.sh [WORK_DIR]
# The script downloads JerryScript v3.0.0 into WORK_DIR (a temporary folder by
# default) and uses every .js file under tests/ as a seed. It builds the
# JavaScript grammar with make in the repository folder. This build replaces
# an earlier build there. Then it runs tests/g4/roundtrip.sh with
# ALLOW_SKIPS=1. ANTLR may reject some files. The script fails if a file has
# no match or if a tree differs from its seed.
set -eu

url=https://github.com/jerryscript-project/jerryscript/archive/refs/tags/v3.0.0.tar.gz
sha256=4d586d922ba575d95482693a45169ebe6cb539c4b5a0d256a6651a39e47bf0fc
grammar_dir=grammars/g4/javascript
root=$(cd "$(dirname "$0")/../.." && pwd)

if [ $# -gt 1 ]; then
  echo "Usage: $0 [WORK_DIR]" >&2
  exit 2
fi
if [ $# -eq 1 ]; then
  mkdir -p "$1"
  work=$(cd "$1" && pwd)
else
  work=$(mktemp -d)
  trap 'rm -rf "$work"' EXIT
fi
cd "$root"

archive=$work/jerryscript-3.0.0.tar.gz
if [ ! -f "$archive" ]; then
  echo "[*] Downloading $url"
  status=0
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL --retry 3 --retry-delay 5 -o "$archive.part" "$url" || status=$?
  else
    wget -q --tries=3 --waitretry=5 -O "$archive.part" "$url" || status=$?
  fi
  if [ "$status" -ne 0 ]; then
    rm -f "$archive.part"
    echo "[-] Cannot download $url" >&2
    exit 1
  fi
  mv "$archive.part" "$archive"
fi

if command -v sha256sum >/dev/null 2>&1; then
  actual=$(sha256sum "$archive" | cut -d ' ' -f 1)
else
  actual=$(shasum -a 256 "$archive" | cut -d ' ' -f 1)
fi
if [ "$actual" != "$sha256" ]; then
  echo "[-] SHA-256 of $archive is $actual, expected $sha256" >&2
  exit 1
fi

rm -rf "$work/src" "$work/seeds"
mkdir "$work/src" "$work/seeds"
tar -xzf "$archive" -C "$work/src"
(cd "$work/src/jerryscript-3.0.0/tests" && find . -type f -name '*.js' | tar -cf - -T -) |
  (cd "$work/seeds" && tar -xf -)
echo "[*] Seeds: $(find "$work/seeds" -type f | wc -l | tr -d ' ') .js files"

jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
${MAKE:-make} GRAMMAR_FILE="$grammar_dir/JavaScriptLexer.g4 $grammar_dir/JavaScriptParser.g4" \
  G4_SUPPORT_DIR="$grammar_dir/Cpp" -j"$jobs"

ALLOW_SKIPS=1 tests/g4/roundtrip.sh javascript "$work/seeds"
