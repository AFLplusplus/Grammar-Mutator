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
"""Unit tests for grammars/g4_to_json.py.

Run them with `make test-python`. The tests need the generated meta-grammar
parser and the antlr4 package in .venv.
"""

from __future__ import annotations

import contextlib
import io
import json
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path
from typing import Dict, List

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "grammars"))

import g4_to_json
from g4_to_json import START_KEY, G4Error, convert, convert_with_map

# typing.Dict, not dict[...]: this alias runs at import time on Python 3.8.
Rules = Dict[str, List[List[str]]]


class ConverterTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def write(self, name: str, text: str) -> Path:
        path = Path(self.tmp.name) / name
        path.write_text(textwrap.dedent(text))
        return path

    def convert_text(self, text: str, start: str | None = None) -> Rules:
        return convert([self.write("T.g4", text)], start)

    def chars_of(self, rules: Rules, symbol: str) -> list[str]:
        """Return the characters of a lexer rule that is one character set."""
        while len(rules[symbol]) == 1 and rules[symbol][0][0] in rules:
            symbol = rules[symbol][0][0]
        return sorted(alternative[0] for alternative in rules[symbol])

    # -- parser rules ------------------------------------------------------

    def test_tokens_get_a_space_after_them(self) -> None:
        rules = self.convert_text("""
            grammar T;
            s : 'a' B ;
            B : 'b' ;
        """)
        self.assertEqual(rules[START_KEY], [["", "<s>"]])
        self.assertEqual(rules["<s>"], [["a", " ", "<B>", " "]])
        self.assertEqual(rules["<B>"], [["b"]])

    def test_actions_predicates_labels_and_eof_are_ignored(self) -> None:
        rules = self.convert_text("""
            grammar T;
            s[int x] returns [int y] : {p()}? first='a' {act();} EOF # Label ;
        """)
        self.assertEqual(rules["<s>"], [["a", " "]])

    def test_suffixes_become_helper_rules(self) -> None:
        rules = self.convert_text("""
            grammar T;
            s : 'a'? 'b'* 'c'+ ;
        """)
        optional, star, plus = rules["<s>"][0]
        self.assertEqual(rules[optional], [["a", " "], []])
        self.assertEqual(rules[star], [["b", " ", star], []])
        (plus_alternative,) = rules[plus]
        self.assertEqual(plus_alternative[:2], ["c", " "])
        tail = plus_alternative[2]
        self.assertEqual(rules[tail], [["c", " ", tail], []])

    def test_subrules_become_helper_rules(self) -> None:
        rules = self.convert_text("""
            grammar T;
            s : ('a' | 'b') 'c' ;
        """)
        helper, *rest = rules["<s>"][0]
        self.assertEqual(rules[helper], [["a", " "], ["b", " "]])
        self.assertEqual(rest, ["c", " "])

    def test_parser_not_set_and_wildcard_use_visible_tokens(self) -> None:
        rules = self.convert_text("""
            grammar T;
            s : ~A . ;
            A : 'a' ;
            B : 'b' ;
            WS : ' ' -> skip ;
            fragment F : 'f' ;
        """)
        not_a, anything = rules["<s>"][0]
        self.assertEqual(rules[not_a], [["<B>", " "]])
        self.assertEqual(rules[anything], [["<A>", " "], ["<B>", " "]])

    def test_type_command_gives_the_token(self) -> None:
        rules = self.convert_text("""
            grammar T;
            tokens { X }
            s : X ;
            A : 'a' -> type(X) ;
            B : 'b' -> type(X) ;
        """)
        (alternative,) = rules["<s>"]
        self.assertEqual(alternative[1], " ")
        self.assertEqual(rules[alternative[0]], [["<A>"], ["<B>"]])

    def test_default_and_given_start_rule(self) -> None:
        text = """
            grammar T;
            first : 'a' ;
            second : 'b' ;
        """
        self.assertEqual(self.convert_text(text)[START_KEY], [["", "<first>"]])
        self.assertEqual(self.convert_text(text, "second")[START_KEY], [["", "<second>"]])
        with self.assertRaises(G4Error):
            self.convert_text(text, "missing")

    def test_split_lexer_and_parser_grammars(self) -> None:
        lexer = self.write("L.g4", "lexer grammar L;\nA : 'a' ;\n")
        parser = self.write("P.g4", "parser grammar P;\noptions { tokenVocab = L; }\ns : A ;\n")
        rules = convert([lexer, parser])
        self.assertEqual(rules["<s>"], [["<A>", " "]])
        self.assertEqual(rules["<A>"], [["a"]])

    # -- parse map ---------------------------------------------------------

    def test_parse_map_kinds_follow_the_output(self) -> None:
        rules, pmap = convert_with_map(
            [
                self.write(
                    "T.g4",
                    """
            grammar T;
            s : 'a' B ('<x>' | c)* ;
            c : B ;
            B : 'b' ;
        """,
                )
            ]
        )
        self.assertEqual(pmap["kinds"][START_KEY], [["lead", "rule"]])
        self.assertEqual(pmap["kinds"]["<s>"][0][:4], ["literal", "sep", "token", "sep"])
        self.assertEqual(pmap["token_keys"]["<B>"], "B")
        self.assertEqual(pmap["parser_rules"], {"s": "<s>", "c": "<c>"})
        for key, alternatives in pmap["kinds"].items():
            self.assertEqual([len(a) for a in alternatives], [len(a) for a in rules[key]])

    def split(self, lexer: str, parser: str) -> list[Path]:
        """Write a lexer grammar L and a parser grammar T that uses it.

        ANTLR accepts tokens { ... } with type(...) only in a lexer grammar.
        """
        return [
            self.write("L.g4", "lexer grammar L;\n" + lexer),
            self.write("T.g4", "parser grammar T;\noptions { tokenVocab = L; }\n" + parser),
        ]

    def test_parse_map_token_keys_of_type_commands(self) -> None:
        # Y has one producer (A), so the token key is <A>. Z has two producers,
        # so the token key is <Z__type>, which the parser never looks into.
        lexer = "tokens { Y, Z }\nA : 'a' -> type(Y) ;\nB : 'b' -> type(Z) ;\nC : 'c' -> type(Z) ;\n"
        _, pmap = convert_with_map(self.split(lexer, "s : Y Z ;\n"))
        self.assertEqual(pmap["token_keys"], {"<A>": "Y", "<Z__type>": "Z"})
        self.assertNotIn("<Z__type>", pmap["kinds"])

    def test_parse_map_token_keys_of_token_sets(self) -> None:
        _, pmap = convert_with_map([self.write("T.g4", "grammar T;\ns : ~A . ;\nA : 'a' ;\nB : 'b' ;\n")])
        self.assertEqual(pmap["token_keys"], {"<A>": "A", "<B>": "B"})

    def test_parse_map_token_name_is_the_token_type(self) -> None:
        # The tokens of A have the type X, so a reference to A and the
        # wildcard give the same name, in each order.
        for body in ("s : A . ;", "s : . A ;"):
            _, pmap = convert_with_map(self.split("tokens { X }\nA : 'a' -> type(X) ;\n", body + "\n"))
            self.assertEqual(pmap["token_keys"]["<A>"], "X", body)

    def test_parse_map_command_line(self) -> None:
        g4 = self.write("T.g4", "grammar T;\ns : A ;\nA : 'a' ;\n")
        out, map_file = Path(self.tmp.name) / "t.json", Path(self.tmp.name) / "t.map.json"
        with contextlib.redirect_stdout(io.StringIO()):
            code = g4_to_json.main([str(g4), "-o", str(out), "--parse-map", str(map_file)])
        self.assertEqual(code, 0)
        _, expected = convert_with_map([g4])
        self.assertEqual(json.loads(map_file.read_text()), expected)
        self.assertEqual(expected["version"], 1)

    def test_parse_map_split_literal(self) -> None:
        _, pmap = convert_with_map([self.write("T.g4", "grammar T;\ns : '<x>' ;\n")])
        self.assertEqual(pmap["kinds"]["<s>"], [["literal", "literal_cont", "sep"]])

    def test_parse_map_module_names(self) -> None:
        _, combined = convert_with_map([self.write("J.g4", "grammar J;\ns : A ;\nA : 'a' ;\n")])
        self.assertEqual((combined["lexer_module"], combined["parser_module"]), ("JLexer", "JParser"))
        lexer = self.write("L.g4", "lexer grammar L;\nA : 'a' ;\n")
        parser = self.write("P.g4", "parser grammar P;\noptions { tokenVocab = L; }\ns : A ;\n")
        _, split = convert_with_map([lexer, parser])
        self.assertEqual((split["lexer_module"], split["parser_module"], split["start_rule"]), ("L", "P", "s"))

    def test_parse_map_survives_clean(self) -> None:
        rules, pmap = convert_with_map(
            [
                self.write(
                    "T.g4",
                    r"""
            grammar T;
            s : 'a' | U | 'a' ;
            U : [\u2028] ;
        """,
                )
            ]
        )
        self.assertEqual(rules["<s>"], [["a", " "]])
        self.assertEqual(pmap["kinds"]["<s>"], [["literal", "sep"]])

    # -- references --------------------------------------------------------

    def test_undefined_parser_rule_is_an_error(self) -> None:
        with self.assertRaises(G4Error) as caught:
            self.convert_text("grammar T;\ns : missing 'a' ;\n")
        self.assertIn("missing", str(caught.exception))

    def test_parser_grammar_without_its_lexer_is_an_error(self) -> None:
        parser = self.write("P.g4", "parser grammar P;\noptions { tokenVocab = L; }\ns : A ;\n")
        with self.assertRaises(G4Error) as caught:
            convert([parser])
        self.assertIn("A", str(caught.exception))

    def test_declared_imaginary_token_is_not_an_error(self) -> None:
        rules = self.convert_text("""
            grammar T;
            tokens { INDENT }
            s : 'a' | INDENT 'b' ;
        """)
        self.assertEqual(rules["<s>"], [["a", " "]])

    def test_import_without_the_imported_file_is_an_error(self) -> None:
        with self.assertRaises(G4Error) as caught:
            self.convert_text("grammar T;\nimport Common;\ns : ID ;\n")
        self.assertIn("Common", str(caught.exception))

    def test_import_with_the_imported_file_works(self) -> None:
        common = self.write("Common.g4", "lexer grammar Common;\nID : 'i' ;\n")
        main = self.write("T.g4", "grammar T;\nimport Common;\ns : ID ;\n")
        self.assertEqual(convert([main, common])["<s>"], [["<ID>", " "]])

    def test_importing_grammar_wins_in_any_file_order(self) -> None:
        common = self.write("Common.g4", "grammar Common;\nc : 'c' ;\nID : 'common' ;\n")
        main = self.write("T.g4", "grammar T;\nimport Common;\ns : ID ;\nID : 'main' ;\n")
        for paths in ([main, common], [common, main]):
            rules = convert(paths)
            self.assertEqual(rules[START_KEY], [["", "<s>"]])
            self.assertEqual(rules["<ID>"], [["main"]])

    # -- lexer rules -------------------------------------------------------

    def test_character_sets(self) -> None:
        rules = self.convert_text(r"""
            grammar T;
            s : A B C D E ;
            A : [a-c_] ;
            B : 'x'..'z' ;
            C : ~[ -~] ;
            D : [\p{L}] ;
            E : [\t\n] ;
        """)
        chars = {name: self.chars_of(rules, f"<{name}>") for name in "ABCDE"}
        self.assertEqual(chars["A"], ["_", "a", "b", "c"])
        self.assertEqual(chars["B"], ["x", "y", "z"])
        self.assertEqual(chars["C"], ["\t", "\n", "\r"])
        self.assertEqual(len(chars["D"]), 52)
        self.assertEqual(chars["E"], ["\t", "\n"])

    def test_general_category_names_give_their_own_characters(self) -> None:
        rules = self.convert_text(r"""
            grammar T;
            s : A B C D E F ;
            A : [\p{Pc}] ;
            B : [\p{Lu}] ;
            C : [\p{Sc}] ;
            D : [\p{Nd}] ;
            E : [\p{Uppercase_Letter}] ;
            F : [\p{Cc}] ;
        """)
        chars = {name: self.chars_of(rules, f"<{name}>") for name in "ABCDEF"}
        self.assertEqual(chars["A"], ["_"])
        self.assertEqual(chars["B"], [chr(c) for c in range(0x41, 0x5B)])
        self.assertEqual(chars["C"], ["$"])
        self.assertEqual(chars["D"], list("0123456789"))
        self.assertEqual(chars["E"], chars["B"])
        self.assertEqual(chars["F"], ["\t", "\n", "\r"])

    def test_non_ascii_literals_are_removed(self) -> None:
        rules = self.convert_text(r"""
            grammar T;
            s : A | 'x' | 'é' ;
            A : 'a' | '‌' ;
        """)
        self.assertEqual(rules["<s>"], [["<A>", " "], ["x", " "]])
        self.assertEqual(rules["<A>"], [["a"]])

    def test_literal_escapes(self) -> None:
        rules = self.convert_text(r"""
            grammar T;
            s : A ;
            A : '\n' 'A' '\'' '\\' ;
        """)
        self.assertEqual(rules["<A>"], [["\n", "A", "'", "\\"]])

    def test_wildcard_uses_the_whole_pool(self) -> None:
        rules = self.convert_text("""
            grammar T;
            s : A ;
            A : . ;
        """)
        self.assertEqual(self.chars_of(rules, "<A>"), sorted(g4_to_json.CHAR_POOL))

    def test_rest_of_line_tokens_get_a_newline_after_them(self) -> None:
        rules = self.convert_text(r"""
            grammar T;
            s : LINE 'x' ;
            LINE : '#' ~[\r\n]* ;
        """)
        self.assertEqual(rules["<s>"], [["<LINE>", "\n", "x", " "]])

    def test_short_sets_do_not_count_as_rest_of_line(self) -> None:
        rules = self.convert_text("""
            grammar T;
            s : NUM 'x' ;
            NUM : '1' [0-9]* ;
        """)
        self.assertEqual(rules["<s>"], [["<NUM>", " ", "x", " "]])

    # -- names and clean-up ------------------------------------------------

    def test_names_are_unique_without_case(self) -> None:
        rules = self.convert_text("""
            grammar T;
            s : identifier ;
            identifier : Identifier ;
            Identifier : 'i' ;
        """)
        upper = [k.upper() for k in rules]
        self.assertEqual(len(upper), len(set(upper)))
        self.assertIn("<Identifier>", rules)
        (parser_key,) = rules["<s>"][0]
        self.assertNotEqual(parser_key, "<Identifier>")
        self.assertEqual(rules[parser_key], [["<Identifier>", " "]])

    def test_impossible_alternatives_and_unreachable_rules_are_removed(self) -> None:
        rules = self.convert_text(r"""
            grammar T;
            s : 'a' | U ;
            unused : 'b' ;
            U : [\u2028] ;
        """)
        self.assertEqual(rules["<s>"], [["a", " "]])
        self.assertNotIn("<unused>", rules)
        self.assertNotIn("<U>", rules)

    def test_no_finite_test_case_is_an_error(self) -> None:
        with self.assertRaises(G4Error):
            self.convert_text("""
                grammar T;
                s : s 'a' ;
            """)

    def test_literal_that_looks_like_a_key_is_split(self) -> None:
        rules = self.convert_text("""
            grammar T;
            s : '<s>' ;
        """)
        self.assertEqual(rules["<s>"], [["<", "s>", " "]])

    def test_syntax_error_in_the_grammar_file(self) -> None:
        with self.assertRaises(G4Error):
            self.convert_text("grammar T;\ns : 'a' \n")


if __name__ == "__main__":
    unittest.main()
