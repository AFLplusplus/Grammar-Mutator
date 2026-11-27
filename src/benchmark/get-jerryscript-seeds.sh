#!/bin/sh
# Get the JerryScript v3.0.0 test files as seeds for the benchmark program.
# Usage: src/benchmark/get-jerryscript-seeds.sh DEST
# The script downloads JerryScript v3.0.0 and checks its SHA-256. It copies
# each .js file of tests/jerry/ into DEST, except the files in tests/jerry/fail/.
# DEST is one flat folder. A "/" in the path below tests/jerry/ becomes "_".
# DEST must not exist, or it must be empty.
set -eu

url=https://github.com/jerryscript-project/jerryscript/archive/refs/tags/v3.0.0.tar.gz
sha256=4d586d922ba575d95482693a45169ebe6cb539c4b5a0d256a6651a39e47bf0fc

if [ $# -ne 1 ]; then
  echo "Usage: $0 DEST" >&2
  exit 2
fi
dest=$1
if [ -e "$dest" ] && [ -n "$(ls -A "$dest" 2>/dev/null)" ]; then
  echo "[-] $dest exists and is not empty" >&2
  exit 1
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

archive=$work/jerryscript-3.0.0.tar.gz
echo "[*] Downloading $url"
status=0
if command -v curl >/dev/null 2>&1; then
  curl -fsSL --retry 3 --retry-delay 5 -o "$archive.part" "$url" || status=$?
else
  wget -q --tries=3 --waitretry=5 -O "$archive.part" "$url" || status=$?
fi
if [ "$status" -ne 0 ]; then
  echo "[-] Cannot download $url" >&2
  exit 1
fi
mv "$archive.part" "$archive"

if command -v sha256sum >/dev/null 2>&1; then
  actual=$(sha256sum "$archive" | cut -d ' ' -f 1)
else
  actual=$(shasum -a 256 "$archive" | cut -d ' ' -f 1)
fi
if [ "$actual" != "$sha256" ]; then
  echo "[-] SHA-256 of the archive is $actual, expected $sha256" >&2
  exit 1
fi

mkdir "$work/src"
tar -xzf "$archive" -C "$work/src"
jerry=$work/src/jerryscript-3.0.0/tests/jerry
mkdir -p "$dest"
count=0
# JerryScript file names have no newline, so a line loop is safe.
find "$jerry" -type f -name '*.js' ! -path "$jerry/fail/*" | sort >"$work/list"
while IFS= read -r file; do
  rel=${file#"$jerry"/}
  cp "$file" "$dest/$(printf '%s' "$rel" | tr '/' '_')"
  count=$((count + 1))
done <"$work/list"
echo "[+] $count seeds in $dest"
