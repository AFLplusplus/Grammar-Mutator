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

import contextlib
import hashlib
import io
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from collections import Counter
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SEEDS_DIR = ROOT / "grammars" / "g4" / "javascript" / "seeds"
sys.path.insert(0, str(SEEDS_DIR))

import get_seeds

CHANGED = [
    (
        "%OptimizeFunctionOnNextCall(f);",
        '__v8native("OptimizeFunctionOnNextCall")(f);',
    ),
    ("f();\n%DeoptimizeNow();", 'f();\n__v8native("DeoptimizeNow")();'),
    (
        "assertTrue(%HasFastProperties(o));",
        'assertTrue(__v8native("HasFastProperties")(o));',
    ),
    (
        "var s = %GetOptimizationStatus(f);",
        'var s = __v8native("GetOptimizationStatus")(f);',
    ),
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

    def test_exclude_entries_name_suite_files(self) -> None:
        # A wrong entry excludes nothing and gives no error, so check each
        # entry against the suite folders.
        folders = {s.name: s.folders for s in get_seeds.SUITES}
        for entry in get_seeds.EXCLUDE:
            suite, _, rel = entry.partition("/")
            self.assertIn(suite, folders, entry)
            self.assertTrue(rel.endswith(".js"), entry)
            self.assertTrue(
                any(
                    f == "." or rel.startswith(f + "/")
                    for f in folders[suite]
                ),
                entry,
            )

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


GIT = [
    "git",
    "-c",
    "init.defaultBranch=main",
    "-c",
    "user.name=test",
    "-c",
    "user.email=test@example.invalid",
]


def run_git(cwd: Path, *args: str) -> str:
    result = subprocess.run(
        GIT + list(args),
        cwd=str(cwd),
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def make_source(path: Path) -> list[str]:
    """Make a repository with two commits. Return the commit hashes."""
    path.mkdir()
    run_git(path, "init", "-q")
    run_git(path, "config", "uploadpack.allowFilter", "true")
    run_git(path, "config", "uploadpack.allowAnySHA1InWant", "true")
    commits = []
    for n in (1, 2):
        for folder, name in (("a", "x.js"), ("b", "y.js")):
            (path / folder).mkdir(exist_ok=True)
            (path / folder / name).write_text("var v%d = %s;\n" % (n, name))
        run_git(path, "add", ".")
        run_git(path, "commit", "-q", "-m", "c%d" % n)
        commits.append(run_git(path, "rev-parse", "HEAD"))
    return commits


class FetchRepoTest(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = Path(self._tmp.name)
        self.src = self.tmp / "source"
        self.commits = make_source(self.src)
        self.url = "file://%s" % self.src

    def test_fetch_and_update(self) -> None:
        work = self.tmp / "work"
        get_seeds.fetch_repo(self.url, self.commits[0], ["a"], work)
        self.assertEqual((work / "a" / "x.js").read_text(), "var v1 = x.js;\n")
        self.assertFalse((work / "b").exists())
        self.assertEqual(run_git(work, "rev-parse", "HEAD"), self.commits[0])
        # A new commit replaces the stale cache.
        result = get_seeds.fetch_repo(self.url, self.commits[1], ["a"], work)
        self.assertEqual(result, work)
        self.assertEqual((work / "a" / "x.js").read_text(), "var v2 = x.js;\n")
        self.assertEqual(run_git(work, "rev-parse", "HEAD"), self.commits[1])

    def test_cache_hit_does_not_fetch(self) -> None:
        work = self.tmp / "work"
        get_seeds.fetch_repo(self.url, self.commits[1], ["a"], work)
        shutil.rmtree(self.src)
        get_seeds.fetch_repo(self.url, self.commits[1], ["a"], work)
        self.assertEqual(run_git(work, "rev-parse", "HEAD"), self.commits[1])

    def test_missing_path(self) -> None:
        with self.assertRaises(SystemExit) as ctx:
            get_seeds.fetch_repo(
                self.url, self.commits[0], ["missing"], self.tmp / "work2"
            )
        self.assertIn("missing", str(ctx.exception))

    def test_bad_commit(self) -> None:
        real_run = subprocess.run

        def quiet_run(*args, **kwargs):  # hide the error text of git
            if not kwargs.get("capture_output"):
                kwargs.setdefault("stderr", subprocess.DEVNULL)
            return real_run(*args, **kwargs)

        with mock.patch.object(get_seeds.subprocess, "run", quiet_run):
            with self.assertRaises(SystemExit) as ctx:
                get_seeds.fetch_repo(
                    self.url, "1" * 40, ["a"], self.tmp / "work3"
                )
        self.assertIn(self.url, str(ctx.exception))

    def test_ls_tree_failure(self) -> None:
        real_git = get_seeds._git

        def fake_git(work, url, *args, **kwargs):
            if "ls-tree" in args:
                return subprocess.CompletedProcess(args, 1, "", "")
            return real_git(work, url, *args, **kwargs)

        with mock.patch.object(get_seeds, "_git", fake_git):
            with self.assertRaises(SystemExit) as ctx:
                get_seeds.fetch_repo(
                    self.url, self.commits[0], ["a"], self.tmp / "work5"
                )
        message = str(ctx.exception)
        self.assertIn("ls-tree", message)
        self.assertIn(self.url, message)
        self.assertNotIn("missing", message)

    def test_unreadable_head(self) -> None:
        with mock.patch.object(get_seeds, "_head", lambda work, url: ""):
            with self.assertRaises(SystemExit) as ctx:
                get_seeds.fetch_repo(
                    self.url, self.commits[0], ["a"], self.tmp / "work6"
                )
        self.assertIn("cannot read HEAD", str(ctx.exception))

    def test_missing_git(self) -> None:
        with mock.patch.object(get_seeds, "GIT_PROGRAM", "no-such-git-xyz"):
            with self.assertRaises(SystemExit) as ctx:
                get_seeds.fetch_repo(
                    self.url, self.commits[0], ["a"], self.tmp / "work4"
                )
        self.assertIn("Cannot run no-such-git-xyz", str(ctx.exception))

    def test_main_fetch(self) -> None:
        suite = get_seeds.Suite("local", self.url, self.commits[0], "a", (".",))
        dest = self.tmp / "dest"
        out = io.StringIO()
        with mock.patch.object(get_seeds, "SUITES", (suite,)):
            with contextlib.redirect_stdout(out):
                code = get_seeds.main(["fetch", str(dest)])
        self.assertEqual(code, 0)
        self.assertEqual(
            (dest / "seeds" / "local" / "local_x.js").read_text(),
            "var v1 = x.js;\n",
        )
        self.assertIn("local", (dest / "summary.txt").read_text())
        self.assertIn("local", out.getvalue())


SHIMS = SEEDS_DIR / "shims.js"
RUNNER = (
    "const vm = require('vm'), fs = require('fs');"
    "process.argv.slice(1).forEach(function (f) {"
    "vm.runInThisContext(fs.readFileSync(f, 'utf8')); });"
)

# A seed for each name that the filter run with JerryScript added to
# shims.js. The seed throws if the shim is missing or does not do its job.
# A check that needs a newer engine feature runs only if the feature exists.
_HAS_RAB = 'typeof ArrayBuffer.prototype.resize === "function"'
ADDED_NAMES = {
    # test262 harness.
    "isConstructor": "if (isConstructor(Array) !== true) throw 0;"
    "if (isConstructor(Math.max) !== false) throw 0;"
    "if (isConstructor(1) !== false) throw 0;",
    "fnGlobalObject": "if (fnGlobalObject() !== globalThis) throw 0;",
    "decimalToPercentHexString":
    'if (decimalToPercentHexString(10) !== "%0A") throw 0;'
    'if (decimalToPercentHexString(255) !== "%FF") throw 0;'
    "decimalToPercentHexString(null);",
    "testLenientAndStrict":
    'testLenientAndStrict("globalThis.n = (globalThis.n || 0) + 1");'
    "if (globalThis.n !== 2) throw 0;",
    "allowProxyTraps":
    "var h = allowProxyTraps({get: function () { return 5; }});"
    "if (new Proxy({}, h).x !== 5) throw 0;"
    "new Proxy({}, allowProxyTraps());",
    "$MAX_ITERATIONS": "if (!($MAX_ITERATIONS > 0)) throw 0;",
    "TemporalHelpers":
    "TemporalHelpers.assertDuration(1, 2); TemporalHelpers.ISO.plainDate();",
    "ctors": "if (ctors.indexOf(Int8Array) < 0) throw 0;"
    "if (ctors.indexOf(BigInt64Array) < 0) throw 0;",
    "anyTypedArrayConstructors":
    "if (anyTypedArrayConstructors.indexOf(Float64Array) < 0) throw 0;"
    "if (anyTypedArrayConstructors.indexOf(BigUint64Array) < 0) throw 0;",
    "floatArrayConstructors":
    "if (floatArrayConstructors.indexOf(Float32Array) < 0) throw 0;"
    "if (floatArrayConstructors.indexOf(Int8Array) >= 0) throw 0;",
    "typedArrayConstructors":
    "if (typedArrayConstructors.indexOf(Uint8Array) < 0) throw 0;"
    "if (typedArrayConstructors.indexOf(BigInt64Array) >= 0) throw 0;",
    "TypedArray":
    "if (TypedArray !== Object.getPrototypeOf(Int8Array)) throw 0;",
    "testWithIntlConstructors":
    "var n = 0; testWithIntlConstructors(function (C) { n++; });"
    'if (typeof Intl === "object" && n === 0) throw 0;',
    "$": 'if ($ !== $262) throw 0; $.agent.start("x"); $.clearKeptObjects();',
    "$DETACHBUFFER": "var ab = new ArrayBuffer(8); $DETACHBUFFER(ab);"
    "$DETACHBUFFER(1);"
    'if (typeof ab.transfer === "function" && ab.byteLength !== 0) throw 0;',
    "assertThrowsValue":
    "assertThrowsValue(function () { globalThis.r = 1; }, 1);"
    "if (globalThis.r !== 1) throw 0;",
    "CreateResizableArrayBuffer":
    "var b = CreateResizableArrayBuffer(4, 8);"
    "if (%s && (b.byteLength !== 4 || b.maxByteLength !== 8)) throw 0;"
    % _HAS_RAB,
    "CreateRabForTest": "var b = CreateRabForTest(Int16Array);"
    "if (%s && (b.byteLength !== 8 || b.maxByteLength !== 16)) throw 0;"
    "CreateRabForTest({get BYTES_PER_ELEMENT() { throw 1; }});" % _HAS_RAB,
    "MayNeedBigInt":
    "if (MayNeedBigInt(new BigInt64Array(1), 2) !== BigInt(2)) throw 0;"
    "if (MayNeedBigInt(new Int8Array(1), 2) !== 2) throw 0;",
    "ToNumbers": "var a = ToNumbers([BigInt(1), 2]);"
    "if (a[0] !== 1 || a[1] !== 2) throw 0; ToNumbers(null);",
    "CollectValuesAndResize": "var v = [];"
    "if (CollectValuesAndResize(1, v, null, 9, 9) !== true) throw 0;"
    "if (v[0] !== 1) throw 0; CollectValuesAndResize(1, null);"
    "if (%s) { var b = new ArrayBuffer(4, {maxByteLength: 8});"
    " CollectValuesAndResize(2, v, b, 2, 8);"
    " if (b.byteLength !== 8) throw 0; }" % _HAS_RAB,
    # mjsunit and d8.
    "assertMaglevved": "assertMaglevved(function () {});",
    "assertNotMaglevved": "assertNotMaglevved(function () {});",
    "Sandbox": "Sandbox.getAddressOf({});",
    "Worker": 'var w = new Worker("x", {type: "string"});'
    "w.postMessage(1); w.getMessage(); w.terminate();",
    # JSTests: jsc, ChakraCore, Mozilla, and LayoutTests files.
    "assertToStringOrNativeFunction": "assertToStringOrNativeFunction(1, 2);",
    "reportCompare": "reportCompare(1, 2, 3);",
    "testRegExp": "testRegExp([], [], [], [], []);",
    "shouldBeTrue":
    'shouldBeTrue("globalThis.s = 1"); if (globalThis.s !== 1) throw 0;',
    "fiatInt52": "if (fiatInt52(3) !== 3) throw 0;",
    "createGlobalObject":
    "if (createGlobalObject() !== globalThis) throw 0;",
    "runString": 'var o = runString("globalThis.q = 1");'
    "if (o !== globalThis || globalThis.q !== 1) throw 0;",
    "testRunner": "testRunner.dumpAsText(); testRunner.waitUntilDone();",
    "WScript": 'WScript.Echo(1); WScript.LoadScriptFile("x");'
    "if (WScript.Arguments.length !== 0) throw 0;"
    'if (WScript.LoadScript("globalThis.w = 1") !== globalThis) throw 0;'
    "if (globalThis.w !== 1) throw 0; WScript.Quit();",
    "tc": "var t = []; t[tc++] = 1; if (t[0] !== 1) throw 0;",
    "TestCase": 'new TestCase("s", "d", 1, 2);',
}
for _name in ("description", "startTest", "inSection", "enterFunc",
              "printBugNumber", "writeHeaderToLog", "printStatus",
              "AddTestCase", "writeTestCaseResult", "test", "debug",
              "stopTest", "exitFunc"):
    ADDED_NAMES[_name] = "%s(1, 2); globalThis.after = 1;" \
        "if (globalThis.after !== 1) throw 0;" % _name


@unittest.skipIf(shutil.which("node") is None, "node is not installed")
class ShimsTest(unittest.TestCase):
    def run_full(self, source: str, pre: str = ""):
        with tempfile.TemporaryDirectory() as tmp:
            seed = Path(tmp) / "seed.js"
            seed.write_text(source)
            files = [str(SHIMS), str(seed)]
            if pre:
                first = Path(tmp) / "pre.js"
                first.write_text(pre)
                files.insert(0, str(first))
            return subprocess.run(
                ["node", "-e", RUNNER] + files,
                capture_output=True,
                text=True,
                timeout=10,
            )

    def run_seed(self, source: str, pre: str = "") -> int:
        return self.run_full(source, pre).returncode

    def test_size(self):
        self.assertLess(len(SHIMS.read_bytes()), 10240)

    def test_test262(self):
        self.assertEqual(
            self.run_seed(
                "assert.sameValue(1, 2); assert(false);"
                "assert.throws(TypeError, function () { globalThis.ran = 1; });"
                "if (globalThis.ran !== 1) throw 0;"
            ),
            0,
        )

    def test_typed_arrays(self):
        self.assertEqual(
            self.run_seed(
                "testWithTypedArrayConstructors(function (TA) {"
                " globalThis.n = (globalThis.n || 0) + 1; });"
                "if (!(globalThis.n > 0)) throw 0;"
            ),
            0,
        )

    def test_262_object(self):
        self.assertEqual(
            self.run_seed(
                "$DONE(); var r = $262.createRealm();"
                "$262.detachArrayBuffer(new ArrayBuffer(8)); $262.gc();"
            ),
            0,
        )

    def test_mjsunit(self):
        self.assertEqual(
            self.run_seed(
                "assertEquals(1, 2); assertTrue(false);"
                "assertThrows(function () { globalThis.t = 1; });"
                "if (globalThis.t !== 1) throw 0;"
                '__v8native("OptimizeFunctionOnNextCall")(function () {});'
            ),
            0,
        )

    def test_mjsunit_string_code(self):
        self.assertEqual(
            self.run_seed(
                'assertThrows("globalThis.u = 1");'
                "if (globalThis.u !== 1) throw 0;"
            ),
            0,
        )

    def test_jstests(self):
        self.assertEqual(
            self.run_seed(
                "noInline(function () {}); $vm.anything(1, 2);"
                "if (numberOfDFGCompiles(function () {}) !== 0) throw 0;"
                "shouldBe(1, 2);"
            ),
            0,
        )

    def test_let_redeclaration(self):
        self.assertEqual(self.run_seed("let assertEquals = 1;"), 0)

    def test_function_declaration_replaces_shim(self):
        self.assertNotEqual(
            self.run_seed(
                "function assertEquals() { throw 1; } assertEquals();"
            ),
            0,
        )

    def test_engine_functions_stay(self):
        pre = "var m = function () {}; globalThis.print = m;" \
            "globalThis.setTimeout = m; globalThis.noInline = m;"
        seed = "if (print !== m || setTimeout !== m || noInline !== m) throw 0;"
        self.assertEqual(self.run_seed(seed, pre), 0)

    def test_without_proxy(self):
        self.assertEqual(
            self.run_seed(
                'if (typeof $vm !== "object") throw 0;',
                "delete globalThis.Proxy;",
            ),
            0,
        )

    def test_async_test_runs_function(self):
        self.assertEqual(
            self.run_seed(
                "asyncTest(function () { globalThis.a = 1;"
                " return Promise.resolve(); });"
                "if (globalThis.a !== 1) throw 0;"
            ),
            0,
        )

    def test_eval_script(self):
        self.assertEqual(
            self.run_seed(
                '$262.evalScript("globalThis.e = 1");'
                "if (globalThis.e !== 1) throw 0;"
                "$262.evalScript({toString: function () { throw 1; }});"
            ),
            0,
        )

    def test_promise_result(self):
        result = self.run_full(
            "assertPromiseResult(Promise.resolve(5),"
            " function (v) { globalThis.v = v; });"
            "setTimeout(function () {"
            ' if (globalThis.v === 5) console.log("V-IS-5"); });'
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("V-IS-5", result.stdout)

    def test_error_without_new(self):
        self.assertEqual(
            self.run_seed(
                'Test262Error("x"); MjsUnitAssertionError("y");'
                "if (globalThis.message !== undefined) throw 0;"
            ),
            0,
        )

    def test_added_names(self):
        # One case for each name that the filter run with JerryScript added.
        # Each seed throws if the shim does not do its job.
        for name, seed in ADDED_NAMES.items():
            with self.subTest(name=name):
                self.assertEqual(self.run_seed(seed), 0)

    def test_added_asserts_always_replaced(self):
        for name in ("assertMaglevved", "assertNotMaglevved",
                     "assertToStringOrNativeFunction", "reportCompare",
                     "testRegExp"):
            with self.subTest(name=name):
                pre = "globalThis.%s = function () { throw 1; };" % name
                self.assertEqual(self.run_seed("%s(1, 2);" % name, pre), 0)

    def test_nested_sink(self):
        self.assertEqual(
            self.run_seed(
                'd8.file.execute("x.js"); $vm.a.b.c(1);'
                'new Sandbox.MemoryView(0, 8); console.log.call(null, "");'
                'if ($vm.a() !== undefined) throw 0;'
            ),
            0,
        )

    def test_console_if_missing(self):
        self.assertEqual(
            self.run_seed(
                'console.log("x"); console.error(1);',
                "delete globalThis.console;",
            ),
            0,
        )

    def test_create_realm_with_engine_realm(self):
        pre = (
            "globalThis.createRealm = function () {"
            ' return {marker: 1, eval: function (s) { return "E:" + s; }};'
            " };"
        )
        seed = (
            "var r = $262.createRealm();"
            "if (r.global.marker !== 1) throw 0;"
            'if (r.evalScript("x") !== "E:x") throw 0;'
            'if ($262.evalScript("1 + 1") !== 2) throw 0;'
            "if (createGlobalObject().marker !== 1) throw 0;"
        )
        self.assertEqual(self.run_seed(seed, pre), 0)

    def test_wscript_timeout_runs_callback(self):
        result = self.run_full(
            'WScript.SetTimeout(function () { console.log("W-RAN"); });'
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("W-RAN", result.stdout)
        pre = "globalThis.setTimeout = function () { throw 1; };"
        self.assertEqual(
            self.run_seed("WScript.SetTimeout(function () {});", pre), 0
        )


class FilterTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = Path(tmp.name)
        self.fake = self.tmp / "fake.sh"
        self.fake.write_text(
            "#!/bin/sh\n"
            "# $1 = shims, $2 = seed\n"
            'seed=${2#--x=}\n'
            'case $(cat "$seed") in\n'
            "  ok) exit 0 ;;\n"
            "  bad) exit 1 ;;\n"
            "  slow) sleep 5 ;;\n"
            "  crash) kill -SEGV $$ ;;\n"
            "esac\n"
        )
        self.fake.chmod(0o755)
        self.shims = self.tmp / "shims.js"
        self.shims.write_text("")
        self.seeds = self.tmp / "SEEDS"
        for suite, name, text in (
            ("mjsunit", "mjsunit_ok.js", "ok"),
            ("mjsunit", "mjsunit_bad.js", "bad"),
            ("mjsunit", "mjsunit_slow.js", "slow"),
            ("jstests", "jstests_crash.js", "crash"),
            ("jstests", "jstests_ok.js", "ok"),
        ):
            (self.seeds / suite).mkdir(parents=True, exist_ok=True)
            (self.seeds / suite / name).write_text(text)
        self.out = self.tmp / "OUT"

    def run_filter(self, template):
        return get_seeds.run_filter(
            template, self.shims, self.seeds, self.out, 1.0, 4
        )

    def test_filter(self):
        summary = self.run_filter(
            ["sh", str(self.fake), "{shims}", "{seed}"]
        )
        found = sorted(
            str(p.relative_to(self.out))
            for p in self.out.rglob("*")
            if p.is_file()
        )
        self.assertEqual(
            found, ["jstests/jstests_ok.js", "mjsunit/mjsunit_ok.js"]
        )
        self.assertEqual(
            (self.out / "mjsunit" / "mjsunit_ok.js").read_bytes(), b"ok"
        )
        self.assertEqual(summary.kept, Counter(mjsunit=1, jstests=1))
        self.assertEqual(summary.failed, Counter(mjsunit=1))
        self.assertEqual(summary.timed_out, Counter(mjsunit=1))
        self.assertEqual(summary.crashed, {"jstests": ["jstests_crash.js"]})
        self.assertEqual(summary.seeds, Counter(mjsunit=3, jstests=2))

    def test_placeholder_in_argument(self):
        summary = self.run_filter(
            ["sh", str(self.fake), "{shims}", "--x={seed}"]
        )
        self.assertEqual(summary.kept, Counter(mjsunit=1, jstests=1))

    def test_non_empty_out(self):
        self.out.mkdir()
        (self.out / "x").write_text("x")
        with self.assertRaises(SystemExit):
            self.run_filter(["sh", str(self.fake), "{shims}", "{seed}"])

    def test_missing_engine(self):
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            code = get_seeds.main(
                [
                    "filter",
                    "--cmd",
                    "no-such-engine-xyz {seed}",
                    str(self.seeds),
                    str(self.tmp / "OUT2"),
                ]
            )
        self.assertEqual(code, 2)
        lines = err.getvalue().splitlines()
        self.assertEqual(len(lines), 1)
        self.assertTrue(lines[0].startswith("[-]"))
        self.assertNotIn("Traceback", err.getvalue())


    def run_main(self, argv):
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            code = get_seeds.main(argv)
        return code, err.getvalue()

    def assert_one_line_error(self, code, text):
        self.assertEqual(code, 2)
        lines = text.splitlines()
        self.assertEqual(len(lines), 1)
        self.assertTrue(lines[0].startswith("[-]"))

    def test_engine_without_exec_bit(self):
        engine = self.tmp / "noexec.sh"
        engine.write_text("#!/bin/sh\nexit 0\n")
        engine.chmod(0o644)
        code, text = self.run_main(
            [
                "filter",
                "--cmd",
                "%s {seed}" % engine,
                str(self.seeds),
                str(self.tmp / "OUT3"),
            ]
        )
        self.assert_one_line_error(code, text)

    def test_non_empty_out_through_main(self):
        self.out.mkdir()
        (self.out / "x").write_text("x")
        code, text = self.run_main(
            ["filter", "--cmd", "no-such-engine-xyz", str(self.seeds),
             str(self.out)]
        )
        self.assert_one_line_error(code, text)
        self.assertIn("empty", text)

    def test_missing_seeds_folder(self):
        code, text = self.run_main(
            ["filter", "--cmd", "sh", str(self.tmp / "none"),
             str(self.tmp / "OUT4")]
        )
        self.assert_one_line_error(code, text)

    def test_shims_path_with_placeholder(self):
        shims = self.tmp / "{seed}" / "shims.js"
        shims.parent.mkdir()
        shims.write_text("")
        fake = self.tmp / "check.sh"
        fake.write_text(
            "#!/bin/sh\n"
            'case $1 in */{seed}/shims.js) ;; *) exit 1 ;; esac\n'
            'case $(cat "$2") in ok) exit 0 ;; *) exit 1 ;; esac\n'
        )
        fake.chmod(0o755)
        summary = get_seeds.run_filter(
            [str(fake), "{shims}", "{seed}"],
            shims,
            self.seeds,
            self.out,
            1.0,
            4,
        )
        self.assertEqual(summary.kept, Counter(mjsunit=1, jstests=1))

    def test_bad_timeout_and_jobs(self):
        for flag, value in (("--timeout", "0"), ("-j", "0")):
            err = io.StringIO()
            with contextlib.redirect_stderr(err):
                with self.assertRaises(SystemExit) as ctx:
                    get_seeds.main(
                        ["filter", "--cmd", "sh", flag, value,
                         str(self.seeds), str(self.tmp / "OUT5")]
                    )
            self.assertEqual(ctx.exception.code, 2)
            self.assertIn(flag, err.getvalue())

    def test_group_kill_reaches_grandchildren(self):
        mark = self.tmp / "mark"
        fake = self.tmp / "grand.sh"
        fake.write_text(
            "#!/bin/sh\n"
            '(sleep 1; touch "$(cat "$2")") &\n'
            "sleep 5\n"
        )
        fake.chmod(0o755)
        seeds = self.tmp / "G"
        (seeds / "mjsunit").mkdir(parents=True)
        (seeds / "mjsunit" / "mjsunit_a.js").write_text(str(mark))
        summary = get_seeds.run_filter(
            [str(fake), "{shims}", "{seed}"],
            self.shims,
            seeds,
            self.tmp / "GOUT",
            0.5,
            1,
        )
        self.assertEqual(summary.timed_out, Counter(mjsunit=1))
        time.sleep(1.5)
        self.assertFalse(mark.exists())

    def test_group_kill_after_normal_exit(self):
        mark = self.tmp / "mark2"
        fake = self.tmp / "leave.sh"
        fake.write_text(
            "#!/bin/sh\n"
            '(sleep 1; touch "$(cat "$2")") &\n'
            "exit 0\n"
        )
        fake.chmod(0o755)
        seeds = self.tmp / "H"
        (seeds / "mjsunit").mkdir(parents=True)
        (seeds / "mjsunit" / "mjsunit_a.js").write_text(str(mark))
        summary = get_seeds.run_filter(
            [str(fake), "{shims}", "{seed}"],
            self.shims,
            seeds,
            self.tmp / "HOUT",
            2.0,
            1,
        )
        self.assertEqual(summary.kept, Counter(mjsunit=1))
        time.sleep(1.5)
        self.assertFalse(mark.exists())


class CountTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = Path(tmp.name)
        self.seeds = self.tmp / "SEEDS"
        for suite, name in (
            ("mjsunit", "mjsunit_ok.js"),
            ("mjsunit", "mjsunit_bad.js"),
            ("mjsunit", "mjsunit_slow.js"),
            ("jstests", "jstests_crash.js"),
            ("jstests", "jstests_ok.js"),
        ):
            (self.seeds / suite).mkdir(parents=True, exist_ok=True)
            (self.seeds / suite / name).write_text("x")

    def test_count_by_suite(self):
        self.assertEqual(
            get_seeds.count_by_suite(self.seeds),
            Counter(mjsunit=3, jstests=2),
        )

    def test_count_table(self):
        trees = self.tmp / "TREES"
        trees.mkdir()
        (trees / "mjsunit_ok.js").write_text("t")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = get_seeds.main(["count", str(self.seeds), str(trees)])
        self.assertEqual(code, 0)
        rows = [line.split() for line in out.getvalue().splitlines()]
        self.assertEqual(rows[0], ["Suite", "Seeds", "Trees"])
        self.assertIn(["jstests", "2", "0"], rows)
        self.assertIn(["mjsunit", "3", "1"], rows)
        self.assertEqual(rows[-1], ["total", "5", "1"])

    def test_missing_folders(self):
        for argv in (
            ["count", str(self.tmp / "none")],
            ["count", str(self.seeds), str(self.tmp / "none")],
        ):
            err = io.StringIO()
            with contextlib.redirect_stderr(err):
                code = get_seeds.main(argv)
            self.assertEqual(code, 2)
            self.assertEqual(len(err.getvalue().splitlines()), 1)
            self.assertTrue(err.getvalue().startswith("[-]"))

    def test_count_without_trees(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            get_seeds.main(["count", str(self.seeds)])
        rows = [line.split() for line in out.getvalue().splitlines()]
        self.assertEqual(rows[0], ["Suite", "Seeds"])
        self.assertEqual(rows[-1], ["total", "5"])


if __name__ == "__main__":
    unittest.main()
