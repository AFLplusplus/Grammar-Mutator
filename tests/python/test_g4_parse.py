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

import copy
import sys
import tempfile
import time
import unittest
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "grammars"))

from f1_c_gen import CFuzzer
from g4_helpers import ROOT, build_parser
from g4_parse import ParseError, TreeBuilder, alternative_order, check_helpers, node_ids
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
        self.assert_reason(TreeBuilder(*self.json), ("﻿" + JSON_INPUT).encode("utf-8"), "syntax error")

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
        builder, _ = self.builder("s : ID* ;\nID : [a-z]+ ;\nWS : ' ' -> skip ;")
        data = " ".join(["abc"] * 20000)
        start = time.monotonic()
        self.round_trip(builder, data)
        self.assertLess(time.monotonic() - start, 10)

    def test_deep_nesting_never_crashes(self) -> None:
        builder, _ = self.builder("e : '(' e ')' | 'x' ;")
        data = "(" * 50000 + "x" + ")" * 50000
        try:
            self.round_trip(builder, data)
        except ParseError as e:
            self.assertEqual(e.reason, "parser limit")

    # -- skips -------------------------------------------------------------

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

    def test_check_helpers_rejects_a_helper_cycle(self) -> None:
        grammar = {"<START>": [["", "<s>"]], "<s>": [["<s__1>", "x"]], "<s__1>": [["<s__1>", "y"], []]}
        kinds = {"<START>": [["lead", "rule"]], "<s>": [["helper", "literal"]], "<s__1>": [["helper", "literal"], []]}
        with self.assertRaises(ValueError):
            check_helpers(grammar, {"kinds": kinds})


if __name__ == "__main__":
    unittest.main()
