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

import sys
import tempfile
import textwrap
import unittest
from pathlib import Path
from typing import Dict, List

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "grammars"))

import g4_to_json
from g4_to_json import START_KEY, G4Error, convert

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
        self.assertEqual(rules[START_KEY], [["<s>"]])
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
        self.assertEqual(self.convert_text(text)[START_KEY], [["<first>"]])
        self.assertEqual(self.convert_text(text, "second")[START_KEY], [["<second>"]])
        with self.assertRaises(G4Error):
            self.convert_text(text, "missing")

    def test_split_lexer_and_parser_grammars(self) -> None:
        lexer = self.write("L.g4", "lexer grammar L;\nA : 'a' ;\n")
        parser = self.write("P.g4", "parser grammar P;\noptions { tokenVocab = L; }\ns : A ;\n")
        rules = convert([lexer, parser])
        self.assertEqual(rules["<s>"], [["<A>", " "]])
        self.assertEqual(rules["<A>"], [["a"]])

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
