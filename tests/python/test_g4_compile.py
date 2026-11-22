#
# american fuzzy lop++ - grammar mutator
# --------------------------------------
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
"""Unit tests for lib/antlr4_shim/g4_compile.sh.

Run them with `make test-python`. The tests run the script with a fake
compiler, so they need only sh.
"""

from __future__ import annotations

import stat
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "lib" / "antlr4_shim" / "g4_compile.sh"
HINT = "G4_SUPPORT_DIR"


class G4CompileTest(unittest.TestCase):
    def setUp(self) -> None:
        self.work = Path(tempfile.mkdtemp())
        self.addCleanup(subprocess.run, ["rm", "-rf", str(self.work)], check=False)
        self.mark = self.work / "hint_mark"
        # A fake compiler that exits with the code in the file "code"
        self.compiler = self.work / "compiler"
        self.compiler.write_text('#!/bin/sh\nexit "$(cat "$(dirname "$0")/code")"\n')
        self.compiler.chmod(self.compiler.stat().st_mode | stat.S_IXUSR)

    def compile(self, source: str, code: int) -> subprocess.CompletedProcess[str]:
        (self.work / "code").write_text(str(code))
        return subprocess.run(
            ["sh", str(SCRIPT), str(self.mark), str(self.compiler), "-O3", "-o", "x.o", "-c", source],
            capture_output=True,
            text=True,
            check=False,
        )

    def test_success_prints_nothing(self) -> None:
        result = self.compile("generated/JavaScriptParser.cpp", 0)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stderr, "")

    def test_failure_keeps_the_exit_code_and_prints_the_hint(self) -> None:
        result = self.compile("generated/JavaScriptParser.cpp", 3)
        self.assertEqual(result.returncode, 3)
        self.assertIn(HINT, result.stderr)

    def test_hint_prints_once_per_mark(self) -> None:
        first = self.compile("generated/JavaScriptParser.cpp", 1)
        second = self.compile("generated/JavaScriptLexer.cpp", 1)
        self.assertIn(HINT, first.stderr)
        self.assertEqual(second.returncode, 1)
        self.assertNotIn(HINT, second.stderr)

    def test_no_hint_for_the_parse_tables(self) -> None:
        result = self.compile("/build/lib/generated/g4_grammar.cpp", 2)
        self.assertEqual(result.returncode, 2)
        self.assertNotIn(HINT, result.stderr)
        # A later failure of a parser file still prints the hint
        result = self.compile("generated/JavaScriptParser.cpp", 1)
        self.assertIn(HINT, result.stderr)


if __name__ == "__main__":
    unittest.main()
