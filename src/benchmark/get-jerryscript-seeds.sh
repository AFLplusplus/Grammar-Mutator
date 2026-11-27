#!/bin/sh
# Get the JerryScript v3.0.0 test files as seeds for the benchmark program.
# Usage: src/benchmark/get-jerryscript-seeds.sh DEST
# The script downloads JerryScript v3.0.0 and checks its SHA-256. It copies
# each .js file of tests/jerry/ into DEST, except the files in tests/jerry/fail/.
# DEST is one flat folder. A "/" in the path below tests/jerry/ becomes "_".
# DEST must not exist, or it must be empty. The script stops with an error if
# two paths give the same flat name. If the script stops before it ends, also
# after SIGHUP, SIGINT, or SIGTERM, the EXIT trap removes its temporary folder
# and the files it copied, and also DEST if the script made it. The script
# never removes a DEST that existed before, also not a symbolic link.
set -eu

url=https://github.com/jerryscript-project/jerryscript/archive/refs/tags/v3.0.0.tar.gz
sha256=4d586d922ba575d95482693a45169ebe6cb539c4b5a0d256a6651a39e47bf0fc

if [ $# -ne 1 ]; then
  echo "Usage: $0 DEST" >&2
  exit 2
fi
dest=$1
if [ -L "$dest" ] && [ ! -e "$dest" ]; then
  echo "[-] $dest is a symbolic link to nothing" >&2
  exit 1
fi
if [ -e "$dest" ] && [ -n "$(ls -A "$dest" 2>/dev/null)" ]; then
  echo "[-] $dest exists and is not empty" >&2
  exit 1
fi

work=$(mktemp -d)
created=0 # 1 if the script made DEST
started=0 # 1 after the script starts to fill DEST
finished=0 # 1 after all files are copied

# Remove the work folder. Remove what the script made in DEST, unless the
# script finished.
cleanup() {
  if [ "$started" -eq 1 ] && [ "$finished" -eq 0 ]; then
    if [ "$created" -eq 1 ]; then
      rm -rf "$dest"
    else
      while IFS= read -r copied; do
        rm -f "$copied"
      done <"$work/copied"
    fi
  fi
  rm -rf "$work"
}
trap cleanup EXIT
# Some shells (dash) run no EXIT trap on a signal. This makes them run it.
trap 'exit 1' HUP INT TERM

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
if [ ! -e "$dest" ] && [ ! -L "$dest" ]; then created=1; fi
: >"$work/copied"
started=1
mkdir -p "$dest"

count=0
# JerryScript file names have no newline, so a line loop is safe.
find "$jerry" -type f -name '*.js' ! -path "$jerry/fail/*" | sort >"$work/list"
while IFS= read -r file; do
  rel=${file#"$jerry"/}
  target=$dest/$(printf '%s' "$rel" | tr '/' '_')
  if [ -e "$target" ]; then
    echo "[-] Two files give the name $target" >&2
    exit 1
  fi
  printf '%s\n' "$target" >>"$work/copied"
  cp "$file" "$target" || {
    echo "[-] Cannot copy $file to $target" >&2
    exit 1
  }
  count=$((count + 1))
done <"$work/list"
finished=1
echo "[+] $count seeds in $dest"
