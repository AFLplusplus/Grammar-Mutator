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
"""Paths for the Python tests of the .g4 tools.

The tests run the ANTLR jar with java. Set ANTLR_JAR to the path of the jar.
The default is antlr-4.13.2-complete.jar in the repository root. `make
test-python` sets ANTLR_JAR.
"""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_ANTLR_JAR = ROOT / "antlr-4.13.2-complete.jar"
