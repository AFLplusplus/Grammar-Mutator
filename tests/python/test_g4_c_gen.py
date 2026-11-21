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
"""Unit tests for grammars/g4_c_gen.py.

Run them with `make test-python`. Some tests need java, the ANTLR jar (see
g4_helpers.py), and a C++ compiler. They skip if one of these is missing.
"""

from __future__ import annotations

import copy
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest
import zipfile
from pathlib import Path
from typing import Any, Dict, List

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "grammars"))

from f1_c_gen import CFuzzer
import g4_c_gen
from g4_c_gen import GrammarError, check_helpers, cpp_method_name, cpp_string, f1_order, generate
from g4_helpers import DEFAULT_ANTLR_JAR, ROOT
import g4_to_json
from g4_to_json import convert_with_map

SCRIPT = ROOT / "grammars" / "g4_c_gen.py"
JSON_G4 = ROOT / "grammars" / "g4" / "JSON.g4"
JS_G4 = [ROOT / "grammars" / "g4" / "javascript" / f"JavaScript{part}.g4" for part in ("Lexer", "Parser")]
RUNTIME = ROOT / "third_party" / "antlr4-cpp-runtime"

# typing.Dict, not dict[...]: this alias runs at import time on Python 3.8.
Rules = Dict[str, List[List[str]]]

# The kinds that refer to a key, and the kinds that hold text
KEY_KINDS = ("Rule", "Token", "Helper")
SYMBOL = re.compile(r'\{::g4::Kind::(\w+), (\d+), \{"((?:[^"\\]|\\.)*)", (\d+)\}\}')
ALTERNATIVE = re.compile(r"\{(g4_k\d+_r\d+|nullptr), (\d+)\}")
KEY = re.compile(r'\{\{"((?:[^"\\]|\\.)*)", (\d+)\}, (g4_k\d+|nullptr), (\d+), \{"((?:[^"\\]|\\.)*)", (\d+)\}\}')


def unescape(text: str) -> bytes:
    """Return the bytes of the body of a C++ string literal from g4_c_gen.py."""

    def byte(match: re.Match[str]) -> str:
        escape = match.group(1)
        return chr(int(escape, 8)) if escape[0] in "01234567" else escape

    return re.sub(r"\\([0-7]{3}|.)", byte, text).encode("latin-1")


def read_tables(source: str) -> list[tuple[str, list[list[tuple[str, int, bytes]]], str]]:
    """Read the key table of g4_grammar.cpp back.

    Return, for each key in key order: its name, its alternatives (each a list
    of (kind, key index, text)), and its token type name.
    """
    symbols = {}
    for name, body in re.findall(r"const ::g4::Symbol (g4_k\d+_r\d+)\[\] = \{\n(.*?)\n\};", source, re.DOTALL):
        found = []
        for kind, key_index, text, length in SYMBOL.findall(body):
            data = unescape(text)
            assert len(data) == int(length), (name, text)
            found.append((kind, int(key_index), data))
        symbols[name] = found
    alternatives = {}
    for name, body in re.findall(r"const ::g4::Alternative (g4_k\d+)\[\] = \{(.*?)\};", source, re.DOTALL):
        alternatives[name] = []
        for array, count in ALTERNATIVE.findall(body):
            alternative = symbols[array] if array != "nullptr" else []
            assert len(alternative) == int(count), (name, array)
            alternatives[name].append(alternative)
    (body,) = re.findall(r"const ::g4::Key g4_keys\[\] = \{\n(.*?)\n\};", source, re.DOTALL)
    keys = []
    for name, _length, array, count, token_type, _type_length in KEY.findall(body):
        alts = alternatives[array] if array != "nullptr" else []
        assert len(alts) == int(count), name
        keys.append((unescape(name).decode("utf-8"), alts, unescape(token_type).decode("utf-8")))
    return keys


def symbols_of(keys: list[Any], alternative: list[tuple[str, int, bytes]]) -> list[str]:
    """Return the JSON symbols of an alternative from read_tables."""
    return [keys[index][0] if kind in KEY_KINDS else data.decode("utf-8") for kind, index, data in alternative]


def small_map(kinds: dict[str, list[list[str]]], **fields: object) -> dict[str, Any]:
    """Return a parse map for a combined grammar T with the start rule s."""
    parse_map = {
        "version": 1,
        "start_rule": "s",
        "lexer_module": "TLexer",
        "parser_module": "TParser",
        "parser_rules": {"s": "<s>"},
        "token_keys": {},
        "kinds": kinds,
    }
    parse_map.update(fields)
    return parse_map


def runtime_headers(tmp: Path) -> Path | None:
    """Return the folder of the ANTLR C++ runtime headers, or None."""
    src = RUNTIME / "antlr4-cpp-runtime-src" / "runtime" / "src"
    if src.is_dir():
        return src
    archives = sorted(RUNTIME.glob("antlr4-cpp-runtime-*-source.zip"))
    if not archives:
        return None
    with zipfile.ZipFile(archives[0]) as archive:
        names = [n for n in archive.namelist() if n.startswith("runtime/src/") and n.endswith(".h")]
        archive.extractall(tmp / "runtime", names)
    return tmp / "runtime" / "runtime" / "src"


class TableTest(unittest.TestCase):
    def test_order_matches_f1_c_gen(self) -> None:
        for paths in ([JSON_G4], JS_G4):
            grammar, parse_map = convert_with_map(paths)
            keys = read_tables(generate(grammar, parse_map, "t"))
            fuzzer = CFuzzer(copy.deepcopy(grammar))
            self.assertEqual([name for name, _alts, _type in keys], list(grammar))
            for index, (key, alternatives, _type) in enumerate(keys):
                self.assertEqual(index + 1, fuzzer.k_to_id(key), key)
                if key in parse_map["kinds"]:
                    self.assertEqual([symbols_of(keys, a) for a in alternatives], fuzzer.grammar[key], key)
                else:
                    # The matcher never enters a key that the parse map does
                    # not describe. So it has no alternatives in the tables.
                    self.assertEqual(alternatives, [], key)

    def test_f1_order_covers_lexer_keys(self) -> None:
        # The tables carry no lexer keys. So check f1_order for them directly.
        for paths in ([JSON_G4], JS_G4):
            grammar, parse_map = convert_with_map(paths)
            order = f1_order(grammar)
            fuzzer = CFuzzer(copy.deepcopy(grammar))
            lexer_keys = [key for key in grammar if key not in parse_map["kinds"]]
            self.assertTrue(lexer_keys)
            for key in lexer_keys:
                self.assertEqual([grammar[key][i] for i in order[key]], fuzzer.grammar[key], key)

    def test_start_key_is_the_same_in_both_scripts(self) -> None:
        self.assertEqual(g4_c_gen.START_KEY, g4_to_json.START_KEY)

    def test_generated_code_qualifies_g4_from_the_global_namespace(self) -> None:
        grammar = {"<START>": [["", "<s>"]], "<s>": [["a", " "]]}
        kinds = {"<START>": [["lead", "rule"]], "<s>": [["literal", "sep"]]}
        source = generate(grammar, small_map(kinds), "g4")
        self.assertIn("namespace g4 {", source)
        self.assertIn("::g4::Grammar", source)
        # No g4:: without a leading :: (a namespace g4 would hide the library).
        self.assertIsNone(re.search(r"(?<!:)\bg4::", source))

    def test_include_path_is_a_plain_c_path(self) -> None:
        grammar = {"<START>": [["", "<s>"]], "<s>": [["a", " "]]}
        kinds = {"<START>": [["lead", "rule"]], "<s>": [["literal", "sep"]]}
        for bad in ('a"b.h', "a\\b.h", "a\nb.h", "a\rb.h"):
            with self.assertRaises(GrammarError, msg=repr(bad)):
                generate(grammar, small_map(kinds), "t", lexer_header=bad)
            with self.assertRaises(GrammarError, msg=repr(bad)):
                generate(grammar, small_map(kinds), "t", parser_header=bad)
        source = generate(grammar, small_map(kinds), "t", lexer_header="dir with space/L.h")
        self.assertIn('#include "dir with space/L.h"\n', source)

    def test_rule_ids_follow_f1_order(self) -> None:
        grammar = {"<START>": [["", "<s>"]], "<s>": [["a", " ", "b", " "], ["a", " "]]}
        kinds = {"<START>": [["lead", "rule"]], "<s>": [["literal", "sep", "literal", "sep"], ["literal", "sep"]]}
        keys = read_tables(generate(grammar, small_map(kinds), "t"))
        self.assertEqual(keys[1][0], "<s>")
        self.assertEqual(keys[1][1][0], [("Literal", 0, b"a"), ("Sep", 0, b" ")])
        self.assertEqual(
            keys[1][1][1], [("Literal", 0, b"a"), ("Sep", 0, b" "), ("Literal", 0, b"b"), ("Sep", 0, b" ")]
        )

    def test_symbol_kinds_and_tables(self) -> None:
        grammar = {
            "<START>": [["", "<s>"]],
            "<s>": [["<ID>", " ", "<s__1>"]],
            "<s__1>": [["<", "a>", " ", "<s__1>"], []],
            "<ID>": [["<charset__1>"]],
            "<charset__1>": [["x"]],
        }
        kinds = {
            "<START>": [["lead", "rule"]],
            "<s>": [["token", "sep", "helper"]],
            "<s__1>": [["literal", "literal_cont", "sep", "helper"], []],
        }
        source = generate(grammar, small_map(kinds, token_keys={"<ID>": "ID"}), "t")
        keys = read_tables(source)
        self.assertEqual(keys[0][1], [[("Lead", 0, b""), ("Rule", 1, b"")]])
        self.assertEqual(keys[1][1], [[("Token", 3, b""), ("Sep", 0, b" "), ("Helper", 2, b"")]])
        self.assertEqual(keys[2][1][0], [])
        self.assertEqual(
            keys[2][1][1], [("Literal", 0, b"<"), ("LiteralCont", 0, b"a>"), ("Sep", 0, b" "), ("Helper", 2, b"")]
        )
        self.assertEqual(
            [(name, token_type) for name, _alts, token_type in keys][3:], [("<ID>", "ID"), ("<charset__1>", "")]
        )
        self.assertIn('const ::g4::ParserRule g4_parser_rules[] = {\n    {{"s", 1}, 1},\n};', source)
        self.assertIn("    g4_keys, 5,\n    g4_parser_rules, 1,\n    0,\n", source)

    def test_glue_uses_the_parse_map_and_the_options(self) -> None:
        grammar = {"<START>": [["", "<default>"]], "<default>": [["a", " "]]}
        kinds = {"<START>": [["lead", "rule"]], "<default>": [["literal", "sep"]]}
        parse_map = small_map(kinds, start_rule="default", parser_rules={"default": "<default>"})
        source = generate(grammar, parse_map, "t_x")
        self.assertIn('#include "TLexer.h"\n#include "TParser.h"\n', source)
        self.assertIn("namespace t_x {", source)
        self.assertIn("std::make_unique<TLexer>(input)", source)
        self.assertIn("std::make_unique<TParser>(tokens)", source)
        self.assertIn("static_cast<TParser &>(parser).default_()", source)
        source = generate(grammar, parse_map, "t_x", lexer_header="gen/L.h", parser_header="gen/P.h")
        self.assertIn('#include "gen/L.h"\n#include "gen/P.h"\n', source)

    def test_reserved_start_rule_gets_underscore(self) -> None:
        self.assertEqual(cpp_method_name("default"), "default_")
        self.assertEqual(cpp_method_name("program"), "program")
        # The C++ target also escapes the names of its own methods.
        self.assertEqual(cpp_method_name("reset"), "reset_")
        self.assertEqual(cpp_method_name("NULL"), "NULL_")

    def test_literal_escapes(self) -> None:
        text = '"\\\né?\x00'
        self.assertEqual(cpp_string(text), '{"\\"\\\\\\012\\303\\251\\?\\000", 7}')
        grammar = {"<START>": [["", "<s>"]], "<s>": [[text, " "]]}
        kinds = {"<START>": [["lead", "rule"]], "<s>": [["literal", "sep"]]}
        self.assertIn(cpp_string(text), generate(grammar, small_map(kinds), "t"))
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("no C++ compiler")
        with tempfile.TemporaryDirectory() as tmp:
            program = Path(tmp) / "escape.cpp"
            program.write_text(
                textwrap.dedent(f"""\
                #include <cstdio>
                #include <string_view>
                int main() {{
                  std::string_view text{cpp_string(text)};
                  std::fwrite(text.data(), 1, text.size(), stdout);
                }}
                """)
            )
            subprocess.run([compiler, "-std=gnu++17", "-o", str(Path(tmp) / "escape"), str(program)], check=True)
            output = subprocess.run([str(Path(tmp) / "escape")], check=True, capture_output=True).stdout
        self.assertEqual(output, text.encode("utf-8"))


class CheckTest(unittest.TestCase):
    def test_helpers_are_regular(self) -> None:
        for paths in ([JSON_G4], JS_G4):
            grammar, parse_map = convert_with_map(paths)
            check_helpers(grammar, parse_map)

    def test_check_helpers_rejects_a_self_reference_before_the_end(self) -> None:
        grammar = {"<START>": [["", "<s>"]], "<s>": [["<s__1>", "x"]], "<s__1>": [["<s__1>", "y"], []]}
        kinds = {"<START>": [["lead", "rule"]], "<s>": [["helper", "literal"]], "<s__1>": [["helper", "literal"], []]}
        with self.assertRaisesRegex(GrammarError, "refers to itself"):
            check_helpers(grammar, {"kinds": kinds})

    def test_check_helpers_rejects_a_helper_cycle(self) -> None:
        grammar = {"<START>": [["", "<s>"]], "<s>": [["<a>"]], "<a>": [["<b>"], []], "<b>": [["<a>", "x"]]}
        kinds = {
            "<START>": [["lead", "rule"]],
            "<s>": [["helper"]],
            "<a>": [["helper"], []],
            "<b>": [["helper", "literal"]],
        }
        with self.assertRaisesRegex(GrammarError, "cycle: <a>, <b>"):
            check_helpers(grammar, {"kinds": kinds})

    def test_bad_tables_are_rejected(self) -> None:
        grammar = {"<START>": [["", "<s>"]], "<s>": [["a", " "]]}
        good = {"<START>": [["lead", "rule"]], "<s>": [["literal", "sep"]]}
        cases = [
            ("version 2", small_map(good, version=2)),
            ("no lexer module", small_map(good, lexer_module=None)),
            ("do not agree", small_map({"<START>": [["lead", "rule"]], "<s>": [["literal"]]})),
            ("do not agree", small_map({**good, "<t>": [[]]})),
            ("does not describe", small_map({"<START>": [["lead", "rule"]]})),
            ("kind 'char'", small_map({"<START>": [["lead", "rule"]], "<s>": [["char", "sep"]]})),
            ("not a key", small_map({"<START>": [["lead", "rule"]], "<s>": [["rule", "sep"]]})),
            ("no token type", small_map({"<START>": [["lead", "rule"]], "<s>": [["token", "sep"]]})),
            ("literal_cont", small_map({"<START>": [["lead", "rule"]], "<s>": [["literal_cont", "sep"]]})),
            ("start rule", small_map(good, start_rule="t")),
            ("do not agree", {k: v for k, v in small_map(good).items() if k != "parser_rules"}),
        ]
        for message, parse_map in cases:
            with self.subTest(message), self.assertRaisesRegex(GrammarError, message):
                generate(grammar, parse_map, "t")


class CommandLineTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp)

    def run_script(self, grammar: Rules, parse_map: dict[str, Any], *args: str) -> subprocess.CompletedProcess[str]:
        (self.tmp / "g.json").write_text(json.dumps(grammar))
        (self.tmp / "m.json").write_text(json.dumps(parse_map))
        command = [sys.executable, str(SCRIPT), "--grammar", str(self.tmp / "g.json")]
        command += ["--parse-map", str(self.tmp / "m.json"), *args]
        return subprocess.run(command, capture_output=True, text=True, check=False)

    def test_cli_writes_the_tables(self) -> None:
        grammar = {"<START>": [["", "<s>"]], "<s>": [["a", " "]]}
        kinds = {"<START>": [["lead", "rule"]], "<s>": [["literal", "sep"]]}
        result = self.run_script(grammar, small_map(kinds), "--namespace", "t_x", "-o", str(self.tmp))
        self.assertEqual((result.returncode, result.stderr), (0, ""))
        self.assertEqual((self.tmp / "g4_grammar.cpp").read_text(), generate(grammar, small_map(kinds), "t_x"))

    def test_bad_helper_grammar_exits_1(self) -> None:
        grammar = {"<START>": [["", "<s>"]], "<s>": [["<a>"]], "<a>": [["<b>"], []], "<b>": [["<a>", "x"]]}
        kinds = {
            "<START>": [["lead", "rule"]],
            "<s>": [["helper"]],
            "<a>": [["helper"], []],
            "<b>": [["helper", "literal"]],
        }
        result = self.run_script(grammar, small_map(kinds), "--namespace", "t_x", "-o", str(self.tmp))
        self.assertEqual(result.returncode, 1)
        self.assertIn("[-] the helper keys make a cycle: <a>, <b>", result.stderr)
        self.assertFalse((self.tmp / "g4_grammar.cpp").exists())

    def test_bad_namespace_exits_1(self) -> None:
        grammar = {"<START>": [["", "<s>"]], "<s>": [["a", " "]]}
        kinds = {"<START>": [["lead", "rule"]], "<s>": [["literal", "sep"]]}
        for namespace in ("1x", "a::b", "class"):
            result = self.run_script(grammar, small_map(kinds), "--namespace", namespace, "-o", str(self.tmp))
            self.assertEqual(result.returncode, 1, namespace)
            self.assertIn("namespace", result.stderr)

    def skip_or_fail(self, reason: str) -> None:
        """Skip the test. With REQUIRE_CPP_COMPILE_TEST=1 (the CI), fail it."""
        if os.environ.get("REQUIRE_CPP_COMPILE_TEST") == "1":
            self.fail(f"{reason}, and REQUIRE_CPP_COMPILE_TEST=1")
        self.skipTest(reason)

    def test_generated_cpp_compiles(self) -> None:
        jar = Path(os.environ.get("ANTLR_JAR") or DEFAULT_ANTLR_JAR)
        compiler = shutil.which("c++")
        if shutil.which("java") is None or not jar.is_file() or compiler is None:
            self.skip_or_fail("java, the ANTLR jar, or a C++ compiler is missing")
        headers = runtime_headers(self.tmp)
        if headers is None:
            self.skip_or_fail("the ANTLR C++ runtime source is missing")
        grammar, parse_map = convert_with_map([JSON_G4])
        out = self.tmp / "gen"
        subprocess.run(
            [
                "java",
                "-jar",
                str(jar),
                "-Dlanguage=Cpp",
                "-package",
                "t_json",
                "-Xexact-output-dir",
                "-o",
                str(out),
                str(JSON_G4),
            ],
            check=True,
            capture_output=True,
        )
        result = self.run_script(grammar, parse_map, "--namespace", "t_json", "-o", str(out))
        self.assertEqual(result.returncode, 0, result.stderr)
        # The namespace of the generated code declares its own g4 first. Then an
        # unqualified g4:: in the generated code finds t_json::g4 and does not
        # compile. Only ::g4:: finds the matcher library.
        prelude = self.tmp / "hide_g4.h"
        prelude.write_text("namespace t_json {\nstruct g4 {};\n}  // namespace t_json\n")
        # The runtime headers are system headers: some compilers warn in them
        # (for example gcc -Woverloaded-virtual). The test checks only the
        # warnings in the generated code and in the matcher header.
        compile_command = [compiler, "-std=gnu++17", "-fsyntax-only", "-Wall", "-Wextra", "-Wno-attributes"]
        compile_command += ["-isystem", str(headers)]
        compile_command += ["-I", str(ROOT / "lib" / "antlr4_shim"), "-I", str(ROOT / "include")]
        compile_command += ["-I", str(out), "-include", str(prelude)]
        result = subprocess.run(
            [*compile_command, str(out / "g4_grammar.cpp")], capture_output=True, text=True, check=False
        )
        self.assertEqual((result.returncode, result.stderr), (0, ""))


if __name__ == "__main__":
    unittest.main()
