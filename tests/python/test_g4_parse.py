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
"""Unit tests for grammars/g4_parse.py.

Run them with `make test-python`. The tests need java and the ANTLR jar (see
g4_helpers.py).
"""

from __future__ import annotations

import contextlib
import copy
import io
import json
import os
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path
from typing import Any
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "grammars"))

from f1_c_gen import CFuzzer
from g4_helpers import ROOT, build_parser
from g4_parse import (
    MAX_TREE_DEPTH,
    PARSER_RECURSION_LIMIT,
    GrammarError,
    ParseError,
    TreeBuilder,
    alternative_order,
    check_helpers,
    main,
    node_ids,
    tree_depth,
)
from g4_to_json import convert, convert_with_map
from g4_tree import RULE_ID_UNKNOWN, TERM_ID, Node, deserialize, serialize, text

JSON_G4 = ROOT / "grammars" / "g4" / "JSON.g4"
JS_G4 = [ROOT / "grammars" / "g4" / "javascript" / f"JavaScript{part}.g4" for part in ("Lexer", "Parser")]
JSON_INPUT = '{ "a" : [1, 2.5e3, true, null],\r\n\t"b": {"é": "ü"} }  '


def find(root: Node, node_id: int) -> list[Node]:
    """Return the nodes with an id, in pre-order."""
    found = []
    stack = [root]
    while stack:
        node = stack.pop()
        if node.id == node_id:
            found.append(node)
        stack.extend(reversed(node.children))
    return found


def leaves(root: Node) -> list[str]:
    return [node.val for node in find(root, TERM_ID)]


class TreeBuilderTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.json_tmp = tempfile.TemporaryDirectory()
        cls.json = build_parser(Path(cls.json_tmp.name), {"JSON.g4": JSON_G4.read_text()})

    @classmethod
    def tearDownClass(cls) -> None:
        cls.json_tmp.cleanup()

    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def builder(self, g4: str, lexer: str | None = None) -> tuple[TreeBuilder, dict[str, Any]]:
        """Build a combined grammar T, or a parser grammar T with a lexer grammar L."""
        if lexer is None:
            files = {"T.g4": "grammar T;\n" + g4}
        else:
            files = {
                "L.g4": "lexer grammar L;\n" + lexer,
                "T.g4": "parser grammar T;\noptions { tokenVocab = L; }\n" + g4,
            }
        grammar, parse_map, parser_dir = build_parser(Path(self.tmp.name), files)
        return TreeBuilder(grammar, parse_map, parser_dir), grammar

    def round_trip(self, builder: TreeBuilder, data: str) -> Node:
        tree = builder.build(data.encode("utf-8"))
        self.assertEqual(text(tree), data)
        return tree

    def assert_reason(self, builder: TreeBuilder, data: bytes, reason: str) -> None:
        with self.assertRaises(ParseError) as caught:
            builder.build(data)
        self.assertEqual(caught.exception.reason, reason)

    # -- exact text --------------------------------------------------------

    def test_json_round_trip(self) -> None:
        builder = TreeBuilder(*self.json)
        tree = self.round_trip(builder, JSON_INPUT)
        self.assertEqual(text(deserialize(serialize(tree))), JSON_INPUT)

    def test_bom_is_rejected_not_changed(self) -> None:
        self.assert_reason(TreeBuilder(*self.json), ("\ufeff" + JSON_INPUT).encode("utf-8"), "syntax error")

    def test_leading_text_goes_to_the_lead_leaf(self) -> None:
        builder, _ = self.builder("""
            s : ID+ ;
            ID : [a-z]+ ;
            WS : [ \\t\\r\\n]+ -> skip ;
            COMMENT : '//' ~[\\r\\n]* -> channel(HIDDEN) ;
        """)
        tree = self.round_trip(builder, "  // c\nx  y\n")
        self.assertEqual(tree.children[0].val, "  // c\n")
        self.assertEqual(leaves(tree), ["  // c\n", "x", "  ", "y", "\n"])

    def test_non_ascii_in_tokens_and_comments(self) -> None:
        builder, _ = self.builder("""
            program : statement* EOF ;
            statement : ID '=' STRING ';' ;
            ID : [a-zA-Z_]+ ;
            STRING : '"' ~["]* '"' ;
            COMMENT : '/*' .*? '*/' -> channel(HIDDEN) ;
            WS : [ \\t\\r\\n]+ -> skip ;
        """)
        self.round_trip(builder, 'x = "日本";\n/* ü */ y = "a";')

    def test_empty_input(self) -> None:
        builder, _ = self.builder("""
            s : ID* EOF ;
            ID : [a-z]+ ;
            WS : [ \\t\\r\\n]+ -> skip ;
        """)
        for data in ("", "  \n"):
            tree = self.round_trip(builder, data)
            self.assertEqual(tree.children[0].val, data)

    # -- node ids and alternatives -----------------------------------------

    def test_rule_id_is_the_f1_order(self) -> None:
        builder, grammar = self.builder("s : 'a' 'b' | 'a' ;")
        tree = self.round_trip(builder, "a")
        (s,) = find(tree, node_ids(grammar)["<s>"])
        self.assertEqual(s.rule_id, alternative_order(grammar)["<s>"].index(1))
        self.assertEqual(s.rule_id, 0)

    def test_helpers_take_the_f1_first_alternative(self) -> None:
        # The empty alternative of ID? is first in F1 order. So the first ID?
        # stays empty, and the second one takes the token.
        builder, grammar = self.builder("s : ID? ID? ;\nID : [a-z]+ ;")
        tree = self.round_trip(builder, "a")
        ids = node_ids(grammar)
        (first,) = find(tree, ids["<s__1>"])
        (second,) = find(tree, ids["<s__2>"])
        self.assertEqual(grammar["<s__1>"][alternative_order(grammar)["<s__1>"][0]], [])
        self.assertEqual((first.rule_id, first.children), (0, []))
        self.assertEqual(leaves(second), ["a", ""])

    def test_token_node_has_unknown_rule_id(self) -> None:
        builder, grammar = self.builder("s : ID ;\nID : [a-z]+ ;")
        tree = self.round_trip(builder, "hello")
        (token,) = find(tree, node_ids(grammar)["<ID>"])
        self.assertEqual(token.rule_id, RULE_ID_UNKNOWN)
        self.assertEqual([(c.id, c.val) for c in token.children], [(TERM_ID, "hello")])

    def test_split_literal(self) -> None:
        builder, _ = self.builder("s : '<a>' ID ;\nID : [a-z]+ ;\nWS : ' ' -> skip ;")
        tree = self.round_trip(builder, "<a> b ")
        self.assertEqual(leaves(tree), ["", "<", "a>", " ", "b", " "])

    def test_type_command(self) -> None:
        # Y has one producer (A); Z has two (B and C), so it gets a Z__type key.
        # ANTLR accepts tokens { ... } for type(...) only in a lexer grammar.
        builder, grammar = self.builder(
            "s : Y Z Z ;",
            """
            tokens { Y, Z }
            A : 'a' -> type(Y) ;
            B : 'b' -> type(Z) ;
            C : 'c' -> type(Z) ;
            WS : ' ' -> skip ;
            """,
        )
        tree = self.round_trip(builder, "a b c")
        ids = node_ids(grammar)
        self.assertEqual([leaves(n) for n in find(tree, ids["<A>"])], [["a"]])
        self.assertEqual([leaves(n) for n in find(tree, ids["<Z__type>"])], [["b"], ["c"]])

    def test_token_set(self) -> None:
        builder, _ = self.builder("""
            s : A ~A . ;
            A : 'a' ;
            B : 'b' ;
            C : 'c' ;
            WS : ' ' -> skip ;
        """)
        self.round_trip(builder, "a b c")
        self.round_trip(builder, "a c a")

    def test_left_recursion(self) -> None:
        # ANTLR needs EOF after a left-recursive rule.
        builder, grammar = self.builder("""
            s : e EOF ;
            e : e '+' e | INT ;
            INT : [0-9]+ ;
            WS : ' ' -> skip ;
        """)
        tree = self.round_trip(builder, "1 + 2+3")
        self.assertEqual(len(find(tree, node_ids(grammar)["<e>"])), 5)

    # -- size and depth ----------------------------------------------------

    def test_long_list(self) -> None:
        # A list of n items gives a tree of depth n + 4 here. This is the
        # longest list that MAX_TREE_DEPTH allows.
        builder, _ = self.builder("s : ID* ;\nID : [a-z]+ ;\nWS : ' ' -> skip ;")
        data = " ".join(["abc"] * (MAX_TREE_DEPTH - 4))
        start = time.monotonic()
        tree = self.round_trip(builder, data)
        self.assertLess(time.monotonic() - start, 10)
        self.assertEqual(tree_depth(tree), MAX_TREE_DEPTH)

    def test_too_deep_tree_is_a_parser_limit(self) -> None:
        builder, _ = self.builder("s : ID* ;\nID : [a-z]+ ;\nWS : ' ' -> skip ;")
        self.assertEqual(tree_depth(builder.build(b"a b c")), 7)
        self.assert_reason(builder, " ".join(["abc"] * (MAX_TREE_DEPTH - 3)).encode(), "parser limit")

    def test_deep_nesting_never_crashes(self) -> None:
        builder, _ = self.builder("e : '(' e ')' | 'x' ;")
        data = "(" * 50000 + "x" + ")" * 50000
        try:
            self.round_trip(builder, data)
        except ParseError as e:
            self.assertEqual(e.reason, "parser limit")

    def test_parser_limit_keeps_the_cause(self) -> None:
        # The ANTLR runtime needs at least one Python frame for each level.
        builder, _ = self.builder("e : '(' e ')' | 'x' ;")
        data = "(" * PARSER_RECURSION_LIMIT + "x" + ")" * PARSER_RECURSION_LIMIT
        with self.assertRaises(ParseError) as caught:
            builder.build(data.encode("utf-8"))
        self.assertEqual(caught.exception.reason, "parser limit")
        self.assertIsInstance(caught.exception.__cause__, RecursionError)

    # -- skips -------------------------------------------------------------

    def test_trailing_tokens_are_a_syntax_error(self) -> None:
        builder, _ = self.builder("s : ID ;\nID : [a-z]+ ;\nWS : [ \\t]+ -> skip ;")
        self.round_trip(builder, "a ")
        self.assert_reason(builder, b"a b c", "syntax error")

    def test_missing_parser_dir_raises_at_construction(self) -> None:
        grammar, parse_map, _parser_dir = self.json
        with self.assertRaises(Exception) as caught:
            TreeBuilder(grammar, parse_map, Path(self.tmp.name))
        self.assertNotIsInstance(caught.exception, ParseError)

    def test_missing_start_method_raises_at_construction(self) -> None:
        grammar, parse_map, parser_dir = self.json
        with self.assertRaises(AttributeError):
            TreeBuilder(grammar, {**parse_map, "start_rule": "missing"}, parser_dir)

    def test_unknown_parse_map_version_raises_at_construction(self) -> None:
        grammar, parse_map, parser_dir = self.json
        for version in (None, 2):
            with self.assertRaises(GrammarError):
                TreeBuilder(grammar, {**parse_map, "version": version}, parser_dir)

    def test_builders_do_not_share_modules(self) -> None:
        # Two parsers with a base class of the same name, in two folders
        loaded = []
        for mark in (1, 2):
            tmp = Path(self.tmp.name) / str(mark)
            tmp.mkdir()
            grammar, parse_map, parser_dir = build_parser(
                tmp, {"T.g4": "grammar T;\noptions { superClass = Base; }\ns : ID ;\nID : [a-z]+ ;\n"}
            )
            (parser_dir / "Base.py").write_text(f"from antlr4 import Parser\nclass Base(Parser):\n    MARK = {mark}\n")
            builder = TreeBuilder(grammar, parse_map, parser_dir)
            loaded.append(builder.parser_class.MARK)
            self.assertEqual(text(builder.build(b"ab")), "ab")
            self.assertNotIn(str(parser_dir), sys.path)
            self.assertFalse(
                [m for m in list(sys.modules.values()) if str(parser_dir) in str(getattr(m, "__file__", None))]
            )
        self.assertEqual(loaded, [1, 2])

    def test_thread_settings_come_back_after_a_failure(self) -> None:
        builder = TreeBuilder(*self.json)
        size, limit = threading.stack_size(), sys.getrecursionlimit()
        failure = RuntimeError("can't start new thread")
        with mock.patch("g4_parse.threading.Thread.start", side_effect=failure):  # noqa: SIM117 (Python 3.8)
            with self.assertRaises(RuntimeError) as caught:
                builder.build(b"[]")
        self.assertNotIsInstance(caught.exception, ParseError)
        self.assertEqual((threading.stack_size(), sys.getrecursionlimit()), (size, limit))

    def test_syntax_error(self) -> None:
        builder = TreeBuilder(*self.json)
        self.assert_reason(builder, b'{"a": }', "syntax error")
        self.assert_reason(builder, b'{"a": 1} #', "syntax error")

    def test_not_utf8(self) -> None:
        self.assert_reason(TreeBuilder(*self.json), b"\xff", "not UTF-8")

    def test_no_match(self) -> None:
        # The converter removes the alternative with a non-ASCII literal.
        builder, _ = self.builder("s : 'é' | ID ;\nID : [a-z]+ ;")
        self.round_trip(builder, "x")
        self.assert_reason(builder, "é".encode(), "no match")


class CommandLineTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.dir = Path(self.tmp.name)

    def run_main(
        self, parser_dir: Path, seeds: Path, trees: Path, parse_map: Path, grammar: Path
    ) -> tuple[int, str, str]:
        out, err = io.StringIO(), io.StringIO()
        argv = ["--grammar", str(grammar), "--parse-map", str(parse_map), "--parser-dir", str(parser_dir)]
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = main([*argv, str(seeds), str(trees)])
        return code, out.getvalue(), err.getvalue()

    def write_maps(self, grammar: dict[str, list[list[str]]], parse_map: dict[str, Any]) -> tuple[Path, Path]:
        grammar_file, map_file = self.dir / "grammar.json", self.dir / "parse_map.json"
        grammar_file.write_text(json.dumps(grammar), encoding="utf-8")
        map_file.write_text(json.dumps(parse_map), encoding="utf-8")
        return grammar_file, map_file

    def test_cli_writes_trees_and_reports_skips(self) -> None:
        grammar, parse_map, parser_dir = build_parser(self.dir, {"JSON.g4": JSON_G4.read_text()})
        grammar_file, map_file = self.write_maps(grammar, parse_map)
        seeds, trees = self.dir / "seeds", self.dir / "out" / "trees"
        (seeds / "sub").mkdir(parents=True)
        (seeds / "a.json").write_text('{"a": 1}', encoding="utf-8")
        (seeds / "sub" / "b c.json").write_text("[1, 2]\n", encoding="utf-8")
        (seeds / "bad.json").write_text("{", encoding="utf-8")
        (seeds / "sub" / "a.json").write_text("[]", encoding="utf-8")
        (seeds / "x.bin").write_bytes(b"\xff\xfe")
        # AFL++ changes each space in a queue name to "_", so "b c.json" and
        # "b_c.json" have the same tree name.
        (seeds / "sub" / "b_c.json").write_text("[3]", encoding="utf-8")
        code, out, err = self.run_main(parser_dir, seeds, trees, map_file, grammar_file)
        self.assertEqual(code, 0, err)
        self.assertEqual(sorted(p.name for p in trees.iterdir()), ["a.json", "b_c.json"])
        self.assertEqual(text(deserialize((trees / "a.json").read_bytes())), '{"a": 1}')
        self.assertEqual(text(deserialize((trees / "b_c.json").read_bytes())), "[1, 2]\n")
        self.assertEqual(
            sorted(err.splitlines()),
            [
                "[!] bad.json: syntax error",
                "[!] sub/a.json: duplicate name",
                "[!] sub/b_c.json: duplicate name",
                "[!] x.bin: not UTF-8",
            ],
        )
        self.assertEqual(
            out.strip(),
            "[+] Parsed 2 of 6 files; skipped: not UTF-8 1, syntax error 1, parser limit 0, "
            "duplicate name 2, no match 0",
        )

    def json_cli(self) -> tuple[Path, Path, Path]:
        grammar, parse_map, parser_dir = build_parser(self.dir, {"JSON.g4": JSON_G4.read_text()})
        grammar_file, map_file = self.write_maps(grammar, parse_map)
        return parser_dir, map_file, grammar_file

    def test_cli_reports_parser_limit_and_no_match(self) -> None:
        parser_dir, map_file, grammar_file = self.json_cli()
        seeds = self.dir / "seeds"
        seeds.mkdir()
        (seeds / "deep.json").write_text("[" * MAX_TREE_DEPTH + "]" * MAX_TREE_DEPTH, encoding="utf-8")
        code, out, err = self.run_main(parser_dir, seeds, self.dir / "trees", map_file, grammar_file)
        self.assertEqual((code, err), (0, "[!] deep.json: parser limit\n"))
        self.assertIn("parser limit 1", out)

        other = self.dir / "other"
        other.mkdir()
        grammar, parse_map, parser_dir = build_parser(other, {"T.g4": "grammar T;\ns : 'é' | ID ;\nID : [a-z]+ ;\n"})
        grammar_file, map_file = self.write_maps(grammar, parse_map)
        (seeds / "deep.json").write_text("é", encoding="utf-8")
        code, out, err = self.run_main(parser_dir, seeds, self.dir / "trees", map_file, grammar_file)
        self.assertEqual((code, err), (0, "[!] deep.json: no match\n"))
        self.assertIn("no match 1", out)

    def test_cli_removes_the_old_tree_of_a_skipped_file(self) -> None:
        parser_dir, map_file, grammar_file = self.json_cli()
        seeds, trees = self.dir / "seeds", self.dir / "trees"
        seeds.mkdir()
        (seeds / "a.json").write_text("[]", encoding="utf-8")
        self.assertEqual(self.run_main(parser_dir, seeds, trees, map_file, grammar_file)[0], 0)
        self.assertTrue((trees / "a.json").exists())
        (seeds / "a.json").write_text("[", encoding="utf-8")
        self.assertEqual(self.run_main(parser_dir, seeds, trees, map_file, grammar_file)[0], 0)
        self.assertFalse((trees / "a.json").exists())

    def test_cli_names_that_differ_only_in_case_are_duplicates(self) -> None:
        parser_dir, map_file, grammar_file = self.json_cli()
        seeds = self.dir / "seeds"
        (seeds / "sub").mkdir(parents=True)
        (seeds / "A.json").write_text("[]", encoding="utf-8")
        (seeds / "sub" / "a.json").write_text("[1]", encoding="utf-8")
        code, _out, err = self.run_main(parser_dir, seeds, self.dir / "trees", map_file, grammar_file)
        self.assertEqual((code, err), (0, "[!] sub/a.json: duplicate name\n"))
        self.assertEqual(text(deserialize((self.dir / "trees" / "A.json").read_bytes())), "[]")

    def test_cli_refuses_trees_in_the_seeds_folder(self) -> None:
        parser_dir, map_file, grammar_file = self.json_cli()
        seeds = self.dir / "seeds"
        seeds.mkdir()
        (seeds / "a.json").write_text("[]", encoding="utf-8")
        for trees in (seeds, seeds / "trees", self.dir / "seeds" / ".." / "seeds"):
            code, _out, err = self.run_main(parser_dir, seeds, trees, map_file, grammar_file)
            self.assertEqual(code, 2, trees)
            self.assertIn("[-] TREES_DIR", err)
        self.assertEqual((seeds / "a.json").read_text(encoding="utf-8"), "[]")
        self.assertFalse((seeds / "trees").exists())

    def test_cli_read_and_write_errors_exit_2(self) -> None:
        parser_dir, map_file, grammar_file = self.json_cli()
        seeds, trees = self.dir / "seeds", self.dir / "trees"
        seeds.mkdir()
        (seeds / "a.json").write_text("[]", encoding="utf-8")
        for target in ("pathlib.Path.read_bytes", "pathlib.Path.write_bytes"):
            with mock.patch(target, side_effect=PermissionError(13, "Permission denied")):
                code, _out, err = self.run_main(parser_dir, seeds, trees, map_file, grammar_file)
            self.assertEqual(code, 2, target)
            self.assertIn("a.json", err)
            self.assertNotIn("Traceback", err)

    def test_cli_thread_failure_exits_2(self) -> None:
        parser_dir, map_file, grammar_file = self.json_cli()
        seeds = self.dir / "seeds"
        seeds.mkdir()
        (seeds / "a.json").write_text("[]", encoding="utf-8")
        with mock.patch("g4_parse.threading.Thread.start", side_effect=RuntimeError("can't start new thread")):
            code, _out, err = self.run_main(parser_dir, seeds, self.dir / "trees", map_file, grammar_file)
        self.assertEqual(code, 2)
        self.assertIn("[-] Cannot start the parser thread", err)

    def test_cli_bad_parse_map_does_not_name_g4_support_dir(self) -> None:
        parser_dir, _map_file, grammar_file = self.json_cli()
        grammar, parse_map = convert_with_map([JSON_G4])
        _, map_file = self.write_maps(grammar, {**parse_map, "version": 2})
        seeds = self.dir / "seeds"
        seeds.mkdir()
        code, _out, err = self.run_main(parser_dir, seeds, self.dir / "trees", map_file, grammar_file)
        self.assertEqual(code, 2)
        self.assertIn("[-] The grammar or the parse map is not valid", err)
        self.assertNotIn("G4_SUPPORT_DIR", err)

    @unittest.skipIf(sys.platform == "darwin", "macOS file names must be UTF-8")
    def test_cli_prints_undecodable_file_names(self) -> None:
        parser_dir, map_file, grammar_file = self.json_cli()
        seeds = self.dir / "seeds"
        seeds.mkdir()
        try:
            (Path(os.fsdecode(bytes(seeds) + b"/bad\xff.json"))).write_text("[", encoding="utf-8")
        except OSError:
            self.skipTest("the file system needs UTF-8 file names")
        err = io.TextIOWrapper(io.BytesIO(), encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(err):
            argv = ["--grammar", str(grammar_file), "--parse-map", str(map_file), "--parser-dir", str(parser_dir)]
            code = main([*argv, str(seeds), str(self.dir / "trees")])
        err.flush()
        self.assertEqual(code, 0)
        self.assertIn(b"bad\\xff.json: syntax error", err.buffer.getvalue())

    def test_cli_action_code_without_support_files_names_g4_support_dir(self) -> None:
        grammar, parse_map, parser_dir = build_parser(
            self.dir, {"T.g4": "grammar T;\noptions { superClass = MissingBase; }\ns : ID ;\nID : [a-z]+ ;\n"}
        )
        grammar_file, map_file = self.write_maps(grammar, parse_map)
        seeds = self.dir / "seeds"
        seeds.mkdir()
        code, _out, err = self.run_main(parser_dir, seeds, self.dir / "trees", map_file, grammar_file)
        self.assertEqual(code, 2)
        self.assertIn("MissingBase", err)
        self.assertIn("G4_SUPPORT_DIR", err)

    def test_cli_missing_parser_dir_names_g4_support_dir(self) -> None:
        grammar, parse_map = convert_with_map([JSON_G4])
        grammar_file, map_file = self.write_maps(grammar, parse_map)
        empty = self.dir / "empty"
        empty.mkdir()
        seeds = self.dir / "seeds"
        seeds.mkdir()
        code, _out, err = self.run_main(empty, seeds, self.dir / "trees", map_file, grammar_file)
        self.assertEqual(code, 2)
        self.assertIn("G4_SUPPORT_DIR", err)
        self.assertIn("[-] Cannot load the Python parser in", err)
        self.assertNotIn("Traceback", err)


class OrderTest(unittest.TestCase):
    def test_order_matches_f1_c_gen(self) -> None:
        for paths in ([JSON_G4], JS_G4):
            grammar = convert(paths)
            order = alternative_order(grammar)
            fuzzer = CFuzzer(copy.deepcopy(grammar))
            for key, alternatives in grammar.items():
                self.assertEqual([alternatives[i] for i in order[key]], fuzzer.grammar[key], key)
                self.assertEqual(node_ids(grammar)[key], fuzzer.k_to_id(key), key)

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


if __name__ == "__main__":
    unittest.main()
