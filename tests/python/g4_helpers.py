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
"""Helpers for the tests that need a generated ANTLR Python parser.

The tests run the ANTLR jar with java. Set ANTLR_JAR to the path of the jar.
The default is antlr-4.13.2-complete.jar in the repository root. `make
test-python` sets ANTLR_JAR.
"""

from __future__ import annotations

import os
import subprocess
import sys
import textwrap
from pathlib import Path
from typing import Any, Dict, List

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "grammars"))

from g4_to_json import convert_with_map

ROOT = Path(__file__).resolve().parents[2]
# typing.Dict, not dict[...]: this alias runs at import time on Python 3.8.
Rules = Dict[str, List[List[str]]]
DEFAULT_ANTLR_JAR = ROOT / "antlr-4.13.2-complete.jar"


def build_parser(tmp: Path, g4_files: dict[str, str]) -> tuple[Rules, dict[str, Any], Path]:
    """Write the .g4 files into tmp and build the grammar for them.

    Return the JSON grammar, the parse map, and the folder of the generated
    Python lexer and parser.
    """
    paths = []
    for name, content in g4_files.items():
        path = tmp / name
        path.write_text(textwrap.dedent(content), encoding="utf-8")
        paths.append(path)
    grammar, parse_map = convert_with_map(paths)
    parser_dir = tmp / "parser"
    jar = os.environ.get("ANTLR_JAR") or str(DEFAULT_ANTLR_JAR)
    subprocess.run(
        ["java", "-jar", jar, "-Dlanguage=Python3", "-Xexact-output-dir", "-o", str(parser_dir), *map(str, paths)],
        check=True,
        capture_output=True,
    )
    return grammar, parse_map, parser_dir
