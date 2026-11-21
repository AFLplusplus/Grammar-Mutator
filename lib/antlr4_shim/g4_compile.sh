#!/bin/sh
# Run one compile command for the generated C++ parser of a .g4 grammar.
# Usage: sh g4_compile.sh <hint mark> <compiler> <arguments>...
# If the command fails, the script prints a hint. A grammar with action code
# or a superClass needs the C++ base classes (G4_SUPPORT_DIR). The hint is not
# about g4_grammar.cpp (the parse tables), so a failure of that file gives no
# hint. The script prints the hint once: it makes the folder <hint mark> first.
# The build removes that folder at the start of each build.
mark=$1
shift
"$@" && exit 0
status=$?
# The source file is the argument after -c
source=
previous=
for arg in "$@"; do
  if [ "$previous" = -c ]; then
    source=$arg
  fi
  previous=$arg
done
case $source in
  g4_grammar.cpp | */g4_grammar.cpp) exit "$status" ;;
esac
if mkdir "$mark" 2>/dev/null; then
  echo "[-] Cannot compile the C++ parser of the .g4 grammar. If the grammar has action code or a" \
    "superClass, build with G4_SUPPORT_DIR (see doc/customizing-grammars.md)." >&2
fi
exit "$status"
