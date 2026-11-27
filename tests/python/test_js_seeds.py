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

import hashlib
import sys
import tempfile
import unittest
from collections import Counter
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


class Test262RulesTest(unittest.TestCase):
    def test_negative_phase(self) -> None:
        def src(phase: str) -> str:
            return (
                "/*---\nnegative:\n  phase: %s\n  type: SyntaxError\n"
                "---*/\n" % phase
            )

        drop = get_seeds.test262_drop_reason
        self.assertEqual(drop(src("parse"), "a.js"), "negative-parse")
        self.assertEqual(drop(src("resolution"), "a.js"), "negative-resolution")
        self.assertIsNone(drop(src("runtime"), "a.js"))

    def test_flags_and_fixture(self) -> None:
        drop = get_seeds.test262_drop_reason
        self.assertEqual(
            drop("/*---\nflags: [module]\n---*/", "a.js"), "module"
        )
        self.assertEqual(
            drop("/*---\nflags:\n  - module\n---*/", "a.js"), "module"
        )
        self.assertIsNone(drop("/*---\nflags: [raw]\n---*/", "a.js"))
        self.assertEqual(drop("", "x_FIXTURE.js"), "fixture")

    def test_crlf(self) -> None:
        drop = get_seeds.test262_drop_reason
        neg = "/*---\r\nnegative:\r\n  phase: parse\r\n  type: X\r\n---*/\r\n"
        self.assertEqual(drop(neg, "a.js"), "negative-parse")
        mod = "/*---\r\nflags:\r\n  - module\r\n---*/\r\n"
        self.assertEqual(drop(mod, "a.js"), "module")
        strict = "/*---\r\nflags: [onlyStrict]\r\n---*/\r\n"
        self.assertTrue(get_seeds.test262_only_strict(strict))

    def test_only_strict(self) -> None:
        strict = get_seeds.test262_only_strict
        self.assertTrue(strict("/*---\nflags: [onlyStrict]\n---*/"))
        self.assertTrue(strict("/*---\nflags: [async, onlyStrict]\n---*/"))
        self.assertFalse(strict("/*---\nflags: [noStrict]\n---*/"))
        self.assertFalse(strict("var x;"))


class DropReasonTest(unittest.TestCase):
    def test_paths(self) -> None:
        drop = get_seeds.drop_reason
        self.assertEqual(drop("mjsunit", "mjsunit.js", b""), "harness")
        for rel in ("mozilla/ecma/shell.js", "mozilla/ecma/browser.js"):
            self.assertEqual(drop("jstests", rel, b""), "harness")
        self.assertEqual(
            drop("jstests", "stress/resources/x.js", b""), "resources"
        )

    def test_size(self) -> None:
        drop = get_seeds.drop_reason
        self.assertIsNone(drop("jstests", "stress/a.js", b"x" * 1048576))
        self.assertEqual(
            drop("jstests", "stress/a.js", b"x" * 1048577), "too-large"
        )


class SeedNameTest(unittest.TestCase):
    def test_names(self) -> None:
        name = get_seeds.seed_name
        self.assertEqual(
            name("test262", "built-ins/Array/a b.js"),
            "test262_built-ins_Array_a_b.js",
        )
        self.assertEqual(
            name("jstests", "stress/\u00e9.js"), "jstests_stress___.js"
        )

    def test_long_names(self) -> None:
        a = get_seeds.seed_name("test262", "a" * 300 + "/x.js")
        b = get_seeds.seed_name("test262", "a" * 300 + "/y.js")
        for n in (a, b):
            self.assertLessEqual(len(n.encode()), 255)
            self.assertTrue(n.endswith(".js"))
        self.assertNotEqual(a, b)
        rel = "a" * 300 + "/x.js"
        digest = hashlib.sha1(rel.encode("utf-8", "surrogateescape"))
        self.assertTrue(a.endswith("_%s.js" % digest.hexdigest()[:8]))

    def test_unique_long(self) -> None:
        name = "x" * 252 + ".js"
        self.assertEqual(len(name), 255)
        names = get_seeds.unique_names([name, name, name])
        self.assertEqual(len(set(names)), 3)
        for n in names:
            self.assertLessEqual(len(n.encode()), 255)
            self.assertTrue(n.endswith(".js"))

    def test_unique(self) -> None:
        self.assertEqual(
            get_seeds.unique_names(["a.js", "a.js", "b.js", "a.js"]),
            ["a.js", "a_2.js", "b.js", "a_3.js"],
        )


class BuildSeedsTest(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = Path(self._tmp.name)

    def write(self, root: str, rel: str, data: bytes) -> None:
        path = self.tmp / root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def test_test262(self) -> None:
        self.write("t262", "built-ins/a.js", b"var a;\n")
        self.write(
            "t262",
            "language/b.js",
            b"/*---\nnegative:\n  phase: parse\n  type: SyntaxError\n---*/\n",
        )
        strict = b"/*---\nflags: [onlyStrict]\n---*/\nvar c;\n"
        self.write("t262", "language/c.js", strict)
        out = self.tmp / "seeds"
        summary = get_seeds.build_seeds({"test262": self.tmp / "t262"}, out)
        self.assertEqual(len(list((out / "test262").iterdir())), 2)
        self.assertEqual(
            (out / "test262" / "test262_language_c.js").read_bytes(),
            b'"use strict";\n' + strict,
        )
        self.assertEqual(summary.files["test262"], 3)
        self.assertEqual(summary.seeds["test262"], 2)
        self.assertEqual(
            summary.dropped["test262"], Counter({"negative-parse": 1})
        )

    def test_mjsunit(self) -> None:
        self.write("mj", "a.js", b"// \xff\xfe\n%Foo(x);\n")
        out = self.tmp / "seeds"
        get_seeds.build_seeds({"mjsunit": self.tmp / "mj"}, out)
        self.assertEqual(
            (out / "mjsunit" / "mjsunit_a.js").read_bytes(),
            b'// \xff\xfe\n__v8native("Foo")(x);\n',
        )

    def test_non_empty_folder(self) -> None:
        self.write("mj", "a.js", b"")
        out = self.tmp / "seeds"
        out.mkdir()
        (out / "x").write_bytes(b"")
        with self.assertRaises(SystemExit) as ctx:
            get_seeds.build_seeds({"mjsunit": self.tmp / "mj"}, out)
        self.assertIn(str(out), str(ctx.exception))

    def test_deterministic(self) -> None:
        self.write("jt", "stress/a b.js", b"var a;")
        self.write("jt", "stress/a_b.js", b"var b;")
        roots = {"jstests": self.tmp / "jt"}
        get_seeds.build_seeds(roots, self.tmp / "one")
        get_seeds.build_seeds(roots, self.tmp / "two")

        def snap(d: Path) -> dict:
            return {p.name: p.read_bytes() for p in (d / "jstests").iterdir()}

        self.assertEqual(snap(self.tmp / "one"), snap(self.tmp / "two"))
        self.assertEqual(
            snap(self.tmp / "one"),
            {
                "jstests_stress_a_b.js": b"var a;",
                "jstests_stress_a_b_2.js": b"var b;",
            },
        )

    def test_harness_and_too_large(self) -> None:
        self.write("mj", "mjsunit.js", b"var h;")
        self.write("mj", "a.js", b"var a;")
        self.write("t262", "built-ins/big.js", b"x" * 1048577)
        roots = {"mjsunit": self.tmp / "mj", "test262": self.tmp / "t262"}
        summary = get_seeds.build_seeds(roots, self.tmp / "seeds")
        self.assertEqual(summary.dropped["mjsunit"], Counter({"harness": 1}))
        self.assertEqual(summary.dropped["test262"], Counter({"too-large": 1}))
        self.assertEqual(summary.seeds["mjsunit"], 1)
        self.assertEqual(summary.seeds["test262"], 0)

    def test_exclude(self) -> None:
        self.write("mj", "a.js", b"var a;")
        summary = get_seeds.build_seeds(
            {"mjsunit": self.tmp / "mj"},
            self.tmp / "seeds",
            frozenset({"mjsunit/a.js"}),
        )
        self.assertEqual(summary.seeds["mjsunit"], 0)
        self.assertEqual(summary.dropped["mjsunit"], Counter({"slow-parse": 1}))

    def test_write_summary(self) -> None:
        summary = get_seeds.BuildSummary(
            files=Counter({"test262": 3}),
            seeds=Counter({"test262": 2}),
            dropped={"test262": Counter({"negative-parse": 1})},
        )
        suites = tuple(s for s in get_seeds.SUITES if s.name == "test262")
        path = self.tmp / "SUMMARY.txt"
        get_seeds.write_summary(path, suites, summary)
        text = path.read_text()
        self.assertIn("test262", text)
        self.assertIn(suites[0].commit, text)
        self.assertIn("negative-parse", text)
        self.assertRegex(text, r"files\D*3")
        self.assertRegex(text, r"seeds\D*2")


if __name__ == "__main__":
    unittest.main()
