#!/bin/sh
# Run one compile command for the generated C++ parser of a .g4 grammar.
# Usage: sh g4_compile.sh <compiler> <arguments>...
# If the command fails, the script prints a hint. A grammar with action code
# or a superClass needs the C++ base classes (G4_SUPPORT_DIR).
"$@" && exit 0
status=$?
echo "[-] Cannot compile the C++ parser of the .g4 grammar. If the grammar has action code or a" \
  "superClass, build with G4_SUPPORT_DIR (see doc/customizing-grammars.md)." >&2
exit "$status"
