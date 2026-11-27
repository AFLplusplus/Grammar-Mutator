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
"""Unit tests for grammars/g4/javascript/seeds/get_seeds.py.

Run them with `make test-python`.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SEEDS_DIR = ROOT / "grammars" / "g4" / "javascript" / "seeds"
sys.path.insert(0, str(SEEDS_DIR))

import get_seeds

CHANGED = [
    ("%OptimizeFunctionOnNextCall(f);", '__v8native("OptimizeFunctionOnNextCall")(f);'),
    ("f();\n%DeoptimizeNow();", 'f();\n__v8native("DeoptimizeNow")();'),
    ("assertTrue(%HasFastProperties(o));", 'assertTrue(__v8native("HasFastProperties")(o));'),
    ("var s = %GetOptimizationStatus(f);", 'var s = __v8native("GetOptimizationStatus")(f);'),
    ("return  %Foo(x);", 'return  __v8native("Foo")(x);'),
    ("if (!%IsSmi(x)) {}", 'if (!__v8native("IsSmi")(x)) {}'),
    ("g(a, %_Call(b));", 'g(a, __v8native("_Call")(b));'),
    ("typeof %Foo(x)", 'typeof __v8native("Foo")(x)'),
    ("throw %Foo(x)", 'throw __v8native("Foo")(x)'),
    (
        "switch (v) { case %Foo(x): break; }",
        'switch (v) { case __v8native("Foo")(x): break; }',
    ),
    ("new %Foo(x)", 'new __v8native("Foo")(x)'),
    ("{%Foo(x)}", '{__v8native("Foo")(x)}'),
    ("}\n%Foo(x)", '}\n__v8native("Foo")(x)'),
    ("%A(1); %B(2);", '__v8native("A")(1); __v8native("B")(2);'),
    ("// c\n%Foo(x);", '// c\n__v8native("Foo")(x);'),
    ("\r%Foo(x)", '\r__v8native("Foo")(x)'),
]

UNCHANGED = [
    "a % b(c);",
    "x %foo(y);",
    "10 %Foo(1);",
    "a[i] %f(1);",
    '"%Foo(";',
    "'%Foo(';",
    "`%Foo(`;",
    "// %Foo(x)\n",
    "/* %Foo(x) */",
    '"\\"%Foo(',
    '"%Foo(',
    '"\\"; %Foo(1)"',
    "'\\'; %Foo(1)'",
    "/* %Foo(",
    "myreturn %Foo(x)",
    "% Foo(x);",  # A space after % is not a runtime call.
]


class RewriteNativesTest(unittest.TestCase):
    def test_changed(self) -> None:
        for src, expected in CHANGED:
            with self.subTest(src=src):
                self.assertEqual(get_seeds.rewrite_natives(src), expected)

    def test_unchanged(self) -> None:
        for src in UNCHANGED:
            with self.subTest(src=src):
                self.assertEqual(get_seeds.rewrite_natives(src), src)


if __name__ == "__main__":
    unittest.main()
