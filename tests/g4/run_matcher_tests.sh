#!/bin/sh
# Build and run the tests of the C++ matcher (tests/g4/matcher_test.cpp).
# Usage: tests/g4/run_matcher_tests.sh [<test name>...]
# Run it from the repository root after a build of a .g4 grammar. Set
# BUILD_DIR for CMake. The script needs java, the ANTLR jar, the Python tools
# of the build, and the ANTLR C++ runtime of the build.
#
# For each folder in tests/g4/grammars, the script makes the JSON grammar
# (g4_to_json.py), the parse tables (g4_c_gen.py), and the C++ lexer and parser
# (the jar), all in the C++ namespace t_<folder>. The .h and .cpp files of the
# folder go with them. Then it compiles the test program with the matcher, the
# tree code in src/, and the runtime library, and runs it.
#
# Optional environment variables:
# - ANTLR_JAR_LOCATION: the ANTLR jar.
# - MATCHER_FLAGS: more compiler and linker flags, for example
#   "-fsanitize=address".
# - JOBS: the number of parallel jobs (default: the number of CPUs).
# - ASAN_OPTIONS: the AddressSanitizer options. If MATCHER_FLAGS has
#   -fsanitize=address and ASAN_OPTIONS is empty, the script sets
#   ASAN_OPTIONS=detect_container_overflow=0. The runtime library has no ASan
#   code, so the container check gives false reports for its vectors (on Linux
#   and macOS). If you set ASAN_OPTIONS, add detect_container_overflow=0 to it.
# The paths of the repository and the build must not have spaces.
set -eu

# One grammar folder: run as "$0 --generate <folder>" (see below)
generate() {
  name=$1
  src=$root/tests/g4/grammars/$name
  out=$work/t_$name
  mkdir -p "$out"
  # shellcheck disable=SC2086 # $meta_option is empty or two words
  "$python" "$root/grammars/g4_to_json.py" $meta_option \
    --parse-map "$out/parse_map.json" -o "$out/grammar.json" "$src"/*.g4 > "$out/log" 2>&1
  "$python" "$root/grammars/g4_c_gen.py" --grammar "$out/grammar.json" \
    --parse-map "$out/parse_map.json" --namespace "t_$name" -o "$out" >> "$out/log" 2>&1
  java -jar "$jar" -Dlanguage=Cpp -package "t_$name" -no-listener -Xexact-output-dir \
    -o "$out" "$src"/*.g4 >> "$out/log" 2>&1
  for f in "$src"/*.h "$src"/*.cpp; do
    if [ -f "$f" ]; then
      cp "$f" "$out/"
    fi
  done
}

# One source file: run as "$0 --compile <source> <object> [<include dir>]".
# The include folder is the folder of a generated grammar.
compile() {
  source=$1
  object=$2
  include=${3:-}
  # shellcheck disable=SC2086 # the flag lists have many words
  case $source in
    *.c)
      ${CC:-cc} $common_flags -I"$root/include" -I"$root/third_party/Cyan4973_xxHash" \
        -c "$source" -o "$object"
      ;;
    "$work"/*)
      # Generated code and the base classes of a test grammar: no warnings
      ${CXX:-c++} $cxx_flags -w -I"$root/include" -I"$root/lib/antlr4_shim" -I"$include" \
        -c "$source" -o "$object"
      ;;
    *)
      ${CXX:-c++} $cxx_flags -Wall -Wextra -I"$root/include" -I"$root/lib/antlr4_shim" \
        -c "$source" -o "$object"
      ;;
  esac
}

root=$(pwd)
if [ ! -f "$root/tests/g4/matcher_test.cpp" ]; then
  echo "[-] Run $0 from the repository root" >&2
  exit 2
fi

if [ -n "${BUILD_DIR:-}" ]; then
  build=$(cd "$BUILD_DIR" && pwd)
  python=$build/venv/bin/python
  meta_option="--meta-dir $build/antlr4_meta"
  runtime_src=$build/third_party/antlr4-cpp-runtime/antlr4-cpp-runtime-src/runtime/src
  runtime_lib=$build/third_party/antlr4-cpp-runtime/antlr4-cpp-runtime-build/runtime/libantlr4-runtime.a
  cache_jar=$(sed -n 's/^ANTLR_JAR_LOCATION:[A-Z]*=//p' "$build/CMakeCache.txt" 2>/dev/null || true)
else
  python=$root/.venv/bin/python
  meta_option=""
  runtime_src=$root/third_party/antlr4-cpp-runtime/antlr4-cpp-runtime-src/runtime/src
  runtime_lib=$root/third_party/antlr4-cpp-runtime/libantlr4-runtime.a
  cache_jar=""
fi
jar=${ANTLR_JAR_LOCATION:-}
for candidate in "$root/antlr-4.13.2-complete.jar" /usr/local/lib/antlr-4.13.2-complete.jar "$cache_jar"; do
  if [ -z "$jar" ] && [ -n "$candidate" ] && [ -f "$candidate" ]; then
    jar=$candidate
  fi
done
for f in "$python" "$runtime_lib" "$runtime_src/antlr4-runtime.h" "$jar"; do
  if [ ! -f "$f" ]; then
    echo "[-] Missing $f. Build a .g4 grammar first (and set BUILD_DIR for CMake)." >&2
    exit 2
  fi
done
jobs=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}
common_flags="-O1 -g -fPIC -pthread ${MATCHER_FLAGS:-}"
case " ${MATCHER_FLAGS:-} " in
  *" -fsanitize="*address*)
    if [ -z "${ASAN_OPTIONS:-}" ]; then
      ASAN_OPTIONS=detect_container_overflow=0
      export ASAN_OPTIONS
    fi
    ;;
esac
cxx_flags="-std=gnu++17 -Wno-attributes $common_flags -I$runtime_src"

if [ "${1:-}" = --generate ]; then
  generate "$2"
  exit
fi
if [ "${1:-}" = --compile ]; then
  compile "$2" "$3" "${4:-}"
  exit
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export work

names=$(cd tests/g4/grammars && ls)
echo "[*] Generating the test grammars: $(echo "$names" | tr '\n' ' ')"
if ! printf '%s\n' "$names" | xargs -P "$jobs" -n 1 sh "$0" --generate; then
  for name in $names; do
    if [ -f "$work/t_$name/log" ]; then
      cat "$work/t_$name/log" >&2
    fi
  done
  echo "[-] Cannot generate the test grammars" >&2
  exit 1
fi

# Each line: a source, its object, and an include folder
mkdir "$work/obj"
{
  for name in $names; do
    for f in "$work/t_$name"/*.cpp; do
      echo "$f $work/obj/t_${name}_$(basename "$f" .cpp).o $work/t_$name"
    done
  done
  for f in lib/antlr4_shim/g4_matcher*.cpp tests/g4/matcher_test.cpp; do
    echo "$root/$f $work/obj/$(basename "$f" .cpp).o"
  done
  for f in tree list utils parse_budget; do
    echo "$root/src/$f.c $work/obj/$f.o"
  done
} > "$work/sources"
echo "[*] Compiling $(wc -l < "$work/sources" | tr -d ' ') files"
if ! xargs -P "$jobs" -L 1 sh "$0" --compile < "$work/sources"; then
  echo "[-] Cannot compile the matcher tests" >&2
  exit 1
fi

# shellcheck disable=SC2086 # the flag list has many words
${CXX:-c++} $common_flags -o "$work/matcher_test" "$work"/obj/*.o "$runtime_lib"
"$work/matcher_test" "$@"
