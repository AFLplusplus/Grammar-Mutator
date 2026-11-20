#!/usr/bin/env python3
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
"""Convert ANTLR 4 grammar (.g4) files into the JSON grammar format.

The JSON grammar goes to f1_c_gen.py, which writes the test case generator.
The conversion does these steps:

- It reads the .g4 files with the ANTLR 4 meta-grammar (grammars/antlr4).
- It changes the EBNF of each rule into BNF. Each subrule and each `?`, `*`,
  and `+` becomes a helper rule.
- It changes each lexer rule into a rule over characters. A character set,
  `~` set, or `.` selects one character from a fixed pool of printable ASCII
  characters.
- It puts a space after each token of a parser rule, because the lexer rules
  that skip whitespace do not give one.
- It ignores actions, predicates, labels, options, and EOF.
- It removes the rules that the start rule cannot reach, and the rules that
  cannot give a finite test case.
"""

# The generated ANTLR parser has no type information, so the parse tree
# contexts have the type Any.
# ruff: noqa: ANN401

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Any, Union

if TYPE_CHECKING:
    from collections.abc import Iterable, Sequence

META_DIR = Path(__file__).resolve().parent / "antlr4"
# The default folder of the generated meta-grammar parser
DEFAULT_META_PARSER_DIR = META_DIR / "generated"

# The characters that a character set can give. The JSON grammar format and
# f1_c_gen.py work with ASCII characters only.
CHAR_POOL = "\t\n\r" + "".join(chr(c) for c in range(0x20, 0x7F))

# The text after each token of a parser rule. It is after the token, not before
# it, because some tokens must be at the start of the input (for example "#!").
TOKEN_SEPARATOR = " "
# The text after a token that continues to the end of the line, such as a
# "#!" line or a line comment. A space would put the next token on that line.
LINE_SEPARATOR = "\n"

START_KEY = "<START>"


# ---------------------------------------------------------------------------
# Grammar model
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Literal:
    """A fixed string."""

    text: str


@dataclass(frozen=True)
class RuleRef:
    """A reference to a parser rule, a lexer rule, or a fragment."""

    name: str


@dataclass(frozen=True)
class CharSet:
    """One character from a set. The set contains only CHAR_POOL characters."""

    chars: tuple[str, ...]


@dataclass(frozen=True)
class TokenSet:
    """One token in a parser rule (`.` or `~set`), except the excluded ones."""

    excluded_names: frozenset[str]
    excluded_texts: frozenset[str]


@dataclass(frozen=True)
class Seq:
    """The items one after the other."""

    items: tuple[Node, ...]


@dataclass(frozen=True)
class Choice:
    """One of the alternatives."""

    alternatives: tuple[Node, ...]


@dataclass(frozen=True)
class Repeat:
    """The item with a `?`, `*`, or `+` suffix."""

    item: Node
    suffix: str


# Union, not "|": the type runs at import time, and "|" needs Python 3.10.
Node = Union[Literal, RuleRef, CharSet, TokenSet, Seq, Choice, Repeat]

EMPTY = Seq(())


@dataclass
class Rule:
    """One rule of the grammar."""

    name: str
    is_lexer: bool
    is_fragment: bool
    body: Node
    # True if the lexer rule skips its text or sends it to a hidden channel
    is_hidden: bool = False
    # The token types that `-> type(X)` gives
    token_types: tuple[str, ...] = ()


@dataclass
class Grammar:
    """The rules of all .g4 files."""

    rules: dict[str, Rule]
    parser_rule_order: list[str]
    # The names of the grammars in the .g4 files
    grammar_names: tuple[str, ...] = ()
    # The grammars that `import` names
    imports: tuple[str, ...] = ()
    # The tokens of `tokens { ... }` blocks (imaginary tokens)
    declared_tokens: frozenset[str] = frozenset()


class G4Error(Exception):
    """An error in a .g4 file or in the conversion."""


# ---------------------------------------------------------------------------
# Escape sequences
# ---------------------------------------------------------------------------

SIMPLE_ESCAPES = {"n": "\n", "r": "\r", "t": "\t", "b": "\b", "f": "\f"}


def read_escape(text: str, i: int) -> tuple[str, int]:
    """Read the escape sequence that starts after the backslash at text[i - 1].

    Return the character and the index after the escape sequence.
    """
    c = text[i]
    if c in SIMPLE_ESCAPES:
        return SIMPLE_ESCAPES[c], i + 1
    if c == "u":
        if i + 1 < len(text) and text[i + 1] == "{":
            end = text.index("}", i + 2)
            return chr(int(text[i + 2 : end], 16)), end + 1
        return chr(int(text[i + 1 : i + 5], 16)), i + 5
    return c, i + 1


def unescape_literal(text: str) -> str:
    """Return the value of a STRING_LITERAL such as 'a\\n'."""
    body = text[1:-1]
    out: list[str] = []
    i = 0
    while i < len(body):
        if body[i] == "\\":
            c, i = read_escape(body, i + 1)
            out.append(c)
        else:
            out.append(body[i])
            i += 1
    return "".join(out)


# Approximate Unicode properties (\p{...}) with ASCII characters
ASCII_LETTERS = frozenset(chr(c) for c in range(0x41, 0x5B)) | frozenset(chr(c) for c in range(0x61, 0x7B))
ASCII_DIGITS = frozenset("0123456789")


def property_chars(name: str) -> frozenset[str]:
    """Return the CHAR_POOL characters for a Unicode property such as L."""
    key = name.rsplit("=", maxsplit=1)[-1].lower()
    if key.startswith(("l", "alpha", "id_start", "xid_start")):
        return ASCII_LETTERS
    if key.startswith(("n", "digit")):
        return ASCII_DIGITS
    if key.startswith(("id_continue", "xid_continue", "alnum", "word")):
        return ASCII_LETTERS | ASCII_DIGITS | frozenset("_")
    if key.startswith(("z", "white_space", "space")):
        return frozenset(" ")
    if key.startswith(("p", "s")):
        return frozenset(c for c in CHAR_POOL if not c.isalnum() and not c.isspace())
    return frozenset()


def char_set_members(text: str) -> frozenset[str]:
    """Return the CHAR_POOL characters of a LEXER_CHAR_SET such as [a-z_]."""
    body = text[1:-1]
    members: set[str] = set()
    items: list[str] = []  # single characters, before range handling
    i = 0
    while i < len(body):
        c = body[i]
        if c == "\\":
            nxt = body[i + 1]
            if nxt in "pP" and i + 2 < len(body) and body[i + 2] == "{":
                end = body.index("}", i + 3)
                chars = property_chars(body[i + 3 : end])
                if nxt == "P":
                    chars = frozenset(CHAR_POOL) - chars
                members |= chars
                i = end + 1
                continue
            c, i = read_escape(body, i + 1)
            items.append(c)
            continue
        if c == "-" and items and i + 1 < len(body):
            # A range such as a-z
            lo = items.pop()
            hi_char = body[i + 1]
            if hi_char == "\\":
                hi, i = read_escape(body, i + 2)
            else:
                hi, i = hi_char, i + 2
            members |= {p for p in CHAR_POOL if lo <= p <= hi}
            continue
        items.append(c)
        i += 1
    members |= {c for c in items if c in CHAR_POOL}
    return frozenset(members)


# ---------------------------------------------------------------------------
# Reading .g4 files
# ---------------------------------------------------------------------------


def load_meta_parser(generated: Path) -> tuple[Any, Any]:
    """Import the generated ANTLR 4 meta-grammar lexer and parser."""
    if not (generated / "ANTLRv4Parser.py").exists():
        msg = f"the meta-grammar parser is missing in {generated}; build it first"
        raise G4Error(msg)
    for path in (str(generated), str(META_DIR)):
        if path not in sys.path:
            sys.path.insert(0, path)
    # The generated modules are on sys.path only from here.
    from ANTLRv4Lexer import ANTLRv4Lexer  # noqa: PLC0415
    from ANTLRv4Parser import ANTLRv4Parser  # noqa: PLC0415

    return ANTLRv4Lexer, ANTLRv4Parser


class SyntaxErrorCounter:
    """Collect the syntax errors of the meta-grammar parser."""

    def __init__(self, filename: str) -> None:
        self.filename = filename
        self.errors: list[str] = []

    # The ANTLR runtime calls these methods by their names.
    def syntaxError(  # noqa: N802
        self, _recognizer: object, _symbol: object, line: int, column: int, msg: str, _e: object
    ) -> None:
        self.errors.append(f"{self.filename}:{line}:{column}: {msg}")

    def reportAmbiguity(self, *_args: object) -> None:  # noqa: N802
        pass

    def reportAttemptingFullContext(self, *_args: object) -> None:  # noqa: N802
        pass

    def reportContextSensitivity(self, *_args: object) -> None:  # noqa: N802
        pass


def parse_g4(path: Path, meta_parser_dir: Path) -> Any:
    """Parse a .g4 file and return its grammarSpec parse tree."""
    # Import here, so that --help works without the antlr4 package.
    from antlr4 import CommonTokenStream, FileStream  # noqa: PLC0415

    lexer_class, parser_class = load_meta_parser(meta_parser_dir)
    lexer = lexer_class(FileStream(str(path), encoding="utf-8"))
    parser = parser_class(CommonTokenStream(lexer))
    counter = SyntaxErrorCounter(str(path))
    lexer.removeErrorListeners()
    parser.removeErrorListeners()
    lexer.addErrorListener(counter)
    parser.addErrorListener(counter)
    tree = parser.grammarSpec()
    if counter.errors:
        raise G4Error("\n".join(counter.errors))
    return tree


def suffix_of(ctx: Any) -> str | None:
    """Return '?', '*', or '+' for an ebnfSuffix context (None if absent)."""
    if ctx is None:
        return None
    return ctx.getText()[0]


def with_suffix(node: Node, suffix: str | None) -> Node:
    return node if suffix is None else Repeat(node, suffix)


class ParserRuleReader:
    """Read the body of a parser rule."""

    def block(self, ctx: Any) -> Node:
        return Choice(tuple(self.alternative(a) for a in ctx.altList().alternative()))

    def alternative(self, ctx: Any) -> Node:
        return Seq(tuple(self.element(e) for e in ctx.element()))

    def element(self, ctx: Any) -> Node:
        if ctx.labeledElement() is not None:
            labeled = ctx.labeledElement()
            node = self.atom(labeled.atom()) if labeled.atom() else self.block(labeled.block())
            return with_suffix(node, suffix_of(ctx.ebnfSuffix()))
        if ctx.atom() is not None:
            return with_suffix(self.atom(ctx.atom()), suffix_of(ctx.ebnfSuffix()))
        if ctx.ebnf() is not None:
            ebnf = ctx.ebnf()
            suffix = suffix_of(ebnf.blockSuffix().ebnfSuffix()) if ebnf.blockSuffix() else None
            return with_suffix(self.block(ebnf.block()), suffix)
        # An action or a predicate
        return EMPTY

    def atom(self, ctx: Any) -> Node:
        if ctx.terminalDef() is not None:
            return terminal_def(ctx.terminalDef())
        if ctx.ruleref() is not None:
            return RuleRef(ctx.ruleref().RULE_REF().getText())
        if ctx.notSet() is not None:
            names: set[str] = set()
            texts: set[str] = set()
            for element in set_elements(ctx.notSet()):
                if element.TOKEN_REF() is not None:
                    names.add(element.TOKEN_REF().getText())
                elif element.STRING_LITERAL() is not None:
                    texts.add(unescape_literal(element.STRING_LITERAL().getText()))
            return TokenSet(frozenset(names), frozenset(texts))
        # A wildcard
        return TokenSet(frozenset(), frozenset())


def terminal_def(ctx: Any) -> Node:
    if ctx.TOKEN_REF() is not None:
        name = ctx.TOKEN_REF().getText()
        return EMPTY if name == "EOF" else RuleRef(name)
    return Literal(unescape_literal(ctx.STRING_LITERAL().getText()))


def set_elements(not_set: Any) -> list[Any]:
    if not_set.setElement() is not None:
        return [not_set.setElement()]
    return list(not_set.blockSet().setElement())


class LexerRuleReader:
    """Read the body of a lexer rule."""

    def __init__(self) -> None:
        self.hidden = False
        self.token_types: list[str] = []

    def alt_list(self, ctx: Any) -> Node:
        return Choice(tuple(self.alternative(a) for a in ctx.lexerAlt()))

    def alternative(self, ctx: Any) -> Node:
        if ctx.lexerCommands() is not None:
            for command in ctx.lexerCommands().lexerCommand():
                self.command(command)
        if ctx.lexerElements() is None:
            return EMPTY
        return Seq(tuple(self.element(e) for e in ctx.lexerElements().lexerElement()))

    def command(self, ctx: Any) -> None:
        name = ctx.lexerCommandName().getText()
        argument = ctx.lexerCommandExpr().getText() if ctx.lexerCommandExpr() else None
        if name == "skip" or (name == "channel" and argument not in (None, "0", "DEFAULT_TOKEN_CHANNEL")):
            self.hidden = True
        elif name == "type" and argument is not None:
            self.token_types.append(argument)

    def element(self, ctx: Any) -> Node:
        if ctx.lexerAtom() is not None:
            return with_suffix(self.atom(ctx.lexerAtom()), suffix_of(ctx.ebnfSuffix()))
        if ctx.lexerBlock() is not None:
            node = self.alt_list(ctx.lexerBlock().lexerAltList())
            return with_suffix(node, suffix_of(ctx.ebnfSuffix()))
        # An action or a predicate
        return EMPTY

    def atom(self, ctx: Any) -> Node:
        if ctx.characterRange() is not None:
            return CharSet(tuple(sorted(range_members(ctx.characterRange()))))
        if ctx.terminalDef() is not None:
            return terminal_def(ctx.terminalDef())
        if ctx.notSet() is not None:
            excluded: set[str] = set()
            for element in set_elements(ctx.notSet()):
                excluded |= set_element_members(element)
            return CharSet(tuple(c for c in CHAR_POOL if c not in excluded))
        if ctx.LEXER_CHAR_SET() is not None:
            return CharSet(tuple(sorted(char_set_members(ctx.LEXER_CHAR_SET().getText()))))
        # A wildcard
        return CharSet(tuple(CHAR_POOL))


def range_members(ctx: Any) -> frozenset[str]:
    lo, hi = (unescape_literal(s.getText()) for s in ctx.STRING_LITERAL())
    return frozenset(c for c in CHAR_POOL if lo <= c <= hi)


def set_element_members(ctx: Any) -> frozenset[str]:
    """Return the characters of a set element in a lexer `~set`."""
    if ctx.STRING_LITERAL() is not None:
        return frozenset(unescape_literal(ctx.STRING_LITERAL().getText()))
    if ctx.characterRange() is not None:
        return range_members(ctx.characterRange())
    if ctx.LEXER_CHAR_SET() is not None:
        return char_set_members(ctx.LEXER_CHAR_SET().getText())
    # A token reference: ANTLR allows only single-character rules here. The
    # converter does not resolve them, so the set excludes nothing for them.
    return frozenset()


def read_grammar(paths: Sequence[Path], meta_parser_dir: Path = DEFAULT_META_PARSER_DIR) -> Grammar:
    """Read the rules of all .g4 files."""
    rules: dict[str, Rule] = {}
    parser_rule_order: list[str] = []
    grammar_names: list[str] = []
    imports: list[str] = []
    declared_tokens: set[str] = set()

    def add(rule: Rule, path: Path) -> None:
        if rule.name in rules:
            print(
                f"[!] {path}: rule {rule.name} is defined again; using the first one",
                file=sys.stderr,
            )
            return
        rules[rule.name] = rule
        if not rule.is_lexer:
            parser_rule_order.append(rule.name)

    for path in paths:
        tree = parse_g4(path, meta_parser_dir)
        grammar_names.append(tree.grammarDecl().identifier().getText())
        for prequel in tree.prequelConstruct():
            if prequel.delegateGrammars() is not None:
                # `import A, B=C;`: the last name is the grammar to import
                imports.extend(d.identifier()[-1].getText() for d in prequel.delegateGrammars().delegateGrammar())
            if prequel.tokensSpec() is not None and prequel.tokensSpec().idList() is not None:
                declared_tokens.update(i.getText() for i in prequel.tokensSpec().idList().identifier())
        lexer_specs = []
        for spec in tree.rules().ruleSpec():
            if spec.parserRuleSpec() is not None:
                parser_spec = spec.parserRuleSpec()
                reader = ParserRuleReader()
                labeled_alts = parser_spec.ruleBlock().ruleAltList().labeledAlt()
                body = Choice(tuple(reader.alternative(labeled.alternative()) for labeled in labeled_alts))
                add(Rule(parser_spec.RULE_REF().getText(), False, False, body), path)
            else:
                lexer_specs.append(spec.lexerRuleSpec())
        for mode in tree.modeSpec():
            lexer_specs.extend(mode.lexerRuleSpec())
        for lexer_spec in lexer_specs:
            reader = LexerRuleReader()
            body = reader.alt_list(lexer_spec.lexerRuleBlock().lexerAltList())
            rule = Rule(
                name=lexer_spec.TOKEN_REF().getText(),
                is_lexer=True,
                is_fragment=lexer_spec.FRAGMENT() is not None,
                body=body,
                is_hidden=reader.hidden,
                token_types=tuple(reader.token_types),
            )
            add(rule, path)
    return Grammar(rules, parser_rule_order, tuple(grammar_names), tuple(imports), frozenset(declared_tokens))


def rule_refs(node: Node) -> set[str]:
    """Return the names of all rules that `node` references."""
    if isinstance(node, RuleRef):
        return {node.name}
    if isinstance(node, Seq):
        children: tuple[Node, ...] = node.items
    elif isinstance(node, Choice):
        children = node.alternatives
    elif isinstance(node, Repeat):
        children = (node.item,)
    else:
        return set()
    names: set[str] = set()
    for child in children:
        names |= rule_refs(child)
    return names


def check_references(grammar: Grammar) -> None:
    """Raise G4Error if an import or a rule reference has no definition."""
    missing_imports = sorted(set(grammar.imports) - set(grammar.grammar_names))
    if missing_imports:
        msg = (
            f"the grammar imports {', '.join(missing_imports)}; "
            "add the imported .g4 files to the grammar files (GRAMMAR_FILE)"
        )
        raise G4Error(msg)
    produced = {t for rule in grammar.rules.values() for t in rule.token_types}
    missing: set[str] = set()
    for rule in grammar.rules.values():
        for name in rule_refs(rule.body):
            if name in grammar.rules:
                continue
            is_token = name[0].isupper()
            if not rule.is_lexer and is_token and (name in produced or name in grammar.declared_tokens):
                continue
            missing.add(name)
    if missing:
        msg = (
            f"no rule defines {', '.join(sorted(missing))}; "
            "give all .g4 files of the grammar (for example the lexer and the parser grammar)"
        )
        raise G4Error(msg)
    for name in sorted(grammar.declared_tokens - set(grammar.rules) - produced):
        print(
            f"[!] token {name} has no lexer rule; the alternatives that use it are not generated",
            file=sys.stderr,
        )


# ---------------------------------------------------------------------------
# Writing the JSON grammar
# ---------------------------------------------------------------------------


def key(name: str) -> str:
    return f"<{name}>"


def literal_symbols(text: str) -> list[str]:
    """Return the terminal symbols for a literal.

    The JSON format reads a symbol such as "<a>" as a nonterminal. So a literal
    with this shape becomes two terminals, "<" and "a>", which give the same
    text.
    """
    if len(text) > 1 and text[0] == "<" and text[-1] == ">":
        return [text[0], text[1:]]
    return [text]


def fixed_text(node: Node) -> str | None:
    """Return the text of a rule body that is one literal (None otherwise)."""
    while isinstance(node, (Choice, Seq)):
        items = node.alternatives if isinstance(node, Choice) else node.items
        if len(items) != 1:
            return None
        node = items[0]
    return node.text if isinstance(node, Literal) else None


def ends_at_line_end(node: Node) -> bool:
    """Return True if a lexer rule continues to the end of the line.

    Such a rule ends with a repeated set that contains most characters, but not
    the newline, for example `'//' ~[\r\n]*`.
    """
    if isinstance(node, Choice):
        return any(ends_at_line_end(a) for a in node.alternatives)
    if isinstance(node, Seq):
        return bool(node.items) and ends_at_line_end(node.items[-1])
    if isinstance(node, Repeat) and node.suffix in "*+" and isinstance(node.item, CharSet):
        chars = node.item.chars
        return "\n" not in chars and len(chars) > len(CHAR_POOL) // 2
    return False


class JsonWriter:
    """Change the grammar model into JSON grammar rules (BNF)."""

    def __init__(self, grammar: Grammar) -> None:
        self.grammar = grammar
        self.out: dict[str, list[list[str]]] = {}
        self.helper_counts: dict[str, int] = {}
        self.char_set_keys: dict[tuple[str, ...], str] = {}
        self.token_type_keys: dict[str, str] = {}
        # f1_c_gen.py makes an upper-case C name from each key (for example
        # NODE_IDENTIFIER). So the names must be unique without case, and must
        # not be START or TERM__.
        self.used_names = {"START", "TERM__"}
        self.names: dict[str, str] = {}
        # The lexer rules keep their names. A parser rule such as "identifier"
        # gets a new name if a lexer rule "Identifier" exists.
        for name, _rule in sorted(grammar.rules.items(), key=lambda item: not item[1].is_lexer):
            self.names[name] = self.claim(name)

    # -- names ---------------------------------------------------------------

    def claim(self, base: str) -> str:
        """Return `base`, or `base` with a number, so that the name is unique."""
        name = base
        number = 1
        while name.upper() in self.used_names:
            number += 1
            name = f"{base}_{number}"
        self.used_names.add(name.upper())
        return name

    def rule_key(self, name: str) -> str:
        """Return the key of a grammar rule (a missing rule keeps its name)."""
        return key(self.names.get(name, name))

    def new_helper(self, owner: str) -> str:
        """Return a new key for a helper rule of the grammar rule `owner`."""
        count = self.helper_counts.get(owner, 0) + 1
        self.helper_counts[owner] = count
        return key(self.claim(f"{self.names[owner]}__{count}"))

    # -- rules ---------------------------------------------------------------

    def write(self, start: str) -> dict[str, list[list[str]]]:
        self.out[START_KEY] = [[self.rule_key(start)]]
        for name, rule in self.grammar.rules.items():
            self.out[self.rule_key(name)] = self.alternatives(rule.body, name, in_parser=not rule.is_lexer)
        return self.out

    def alternatives(self, node: Node, owner: str, *, in_parser: bool) -> list[list[str]]:
        if isinstance(node, Choice):
            result: list[list[str]] = []
            for alternative in node.alternatives:
                result.extend(self.alternatives(alternative, owner, in_parser=in_parser))
            return result
        return [self.sequence(node, owner, in_parser=in_parser)]

    def sequence(self, node: Node, owner: str, *, in_parser: bool) -> list[str]:  # noqa: PLR0911
        if isinstance(node, Seq):
            symbols: list[str] = []
            for item in node.items:
                symbols.extend(self.sequence(item, owner, in_parser=in_parser))
            return symbols
        if isinstance(node, Literal):
            if not node.text:
                return []
            text = literal_symbols(node.text)
            return [*text, TOKEN_SEPARATOR] if in_parser else text
        if isinstance(node, RuleRef):
            return self.rule_ref(node.name, in_parser=in_parser)
        if isinstance(node, CharSet):
            return [self.char_set(node.chars)]
        if isinstance(node, TokenSet):
            return [self.token_set(node, owner)]
        if isinstance(node, Choice):
            helper = self.new_helper(owner)
            self.out[helper] = self.alternatives(node, owner, in_parser=in_parser)
            return [helper]
        return [self.repeat(node, owner, in_parser=in_parser)]

    def rule_ref(self, name: str, *, in_parser: bool) -> list[str]:
        rule = self.grammar.rules.get(name)
        is_token = name[0].isupper()
        if not in_parser or not is_token:
            return [self.rule_key(name)]
        # A token in a parser rule. A token can also come from `-> type(X)`.
        producers = [r.name for r in self.grammar.rules.values() if name in r.token_types and not r.is_hidden]
        if rule is not None and not rule.is_hidden:
            producers.insert(0, name)
        if len(producers) == 1:
            return [self.rule_key(producers[0]), self.separator_after(producers[0])]
        if name not in self.token_type_keys:
            helper = key(self.claim(f"{name}__type"))
            self.token_type_keys[name] = helper
            self.out[helper] = [[self.rule_key(p)] for p in producers]  # empty if no producer
        return [self.token_type_keys[name], TOKEN_SEPARATOR]

    def separator_after(self, token: str) -> str:
        """Return the text to put after a token of the lexer rule `token`."""
        rule = self.grammar.rules.get(token)
        if rule is not None and ends_at_line_end(rule.body):
            return LINE_SEPARATOR
        return TOKEN_SEPARATOR

    def char_set(self, chars: tuple[str, ...]) -> str:
        if chars not in self.char_set_keys:
            helper = key(self.claim(f"charset__{len(self.char_set_keys) + 1}"))
            self.char_set_keys[chars] = helper
            self.out[helper] = [[c] for c in chars]  # empty if no character
        return self.char_set_keys[chars]

    def token_set(self, node: TokenSet, owner: str) -> str:
        helper = self.new_helper(owner)
        alternatives: list[list[str]] = []
        for name, rule in self.grammar.rules.items():
            if not rule.is_lexer or rule.is_fragment or rule.is_hidden or name in node.excluded_names:
                continue
            if fixed_text(rule.body) in node.excluded_texts:
                continue
            alternatives.append([self.rule_key(name), self.separator_after(name)])
        self.out[helper] = alternatives
        return helper

    def repeat(self, node: Repeat, owner: str, *, in_parser: bool) -> str:
        item = self.sequence(node.item, owner, in_parser=in_parser)
        helper = self.new_helper(owner)
        if node.suffix == "?":
            self.out[helper] = [item, []]
        elif node.suffix == "*":
            self.out[helper] = [[*item, helper], []]
        else:
            star = self.new_helper(owner)
            self.out[star] = [[*item, star], []]
            self.out[helper] = [[*item, star]]
        return helper


# ---------------------------------------------------------------------------
# Clean-up
# ---------------------------------------------------------------------------


def is_nonterminal(symbol: str, rules: dict[str, list[list[str]]]) -> bool:
    return symbol in rules


def productive_keys(rules: dict[str, list[list[str]]]) -> set[str]:
    """Return the keys that can give a finite string."""
    productive: set[str] = set()
    changed = True
    while changed:
        changed = False
        for k, alternatives in rules.items():
            if k in productive:
                continue
            for alternative in alternatives:
                if all(not is_nonterminal(s, rules) or s in productive for s in alternative):
                    productive.add(k)
                    changed = True
                    break
    return productive


def reachable_keys(rules: dict[str, list[list[str]]], start: str) -> set[str]:
    reachable = {start}
    todo = [start]
    while todo:
        for alternative in rules[todo.pop()]:
            for symbol in alternative:
                if is_nonterminal(symbol, rules) and symbol not in reachable:
                    reachable.add(symbol)
                    todo.append(symbol)
    return reachable


def clean(rules: dict[str, list[list[str]]]) -> dict[str, list[list[str]]]:
    """Remove the alternatives and rules that cannot give a finite string, the
    unreachable rules, and repeated alternatives."""
    productive = productive_keys(rules)
    if START_KEY not in productive:
        msg = "the start rule cannot give a finite test case (check the character pool and the token rules)"
        raise G4Error(msg)
    kept: dict[str, list[list[str]]] = {}
    for k, alternatives in rules.items():
        if k not in productive:
            continue
        seen: set[tuple[str, ...]] = set()
        kept[k] = []
        for alternative in alternatives:
            if any(is_nonterminal(s, rules) and s not in productive for s in alternative):
                continue
            if tuple(alternative) in seen:
                continue
            seen.add(tuple(alternative))
            kept[k].append(alternative)
    reachable = reachable_keys(kept, START_KEY)
    result = {START_KEY: kept[START_KEY]}
    for k, alternatives in kept.items():
        if k in reachable and k != START_KEY:
            result[k] = alternatives
    return result


# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------


def convert(
    paths: Sequence[Path],
    start: str | None = None,
    meta_parser_dir: Path = DEFAULT_META_PARSER_DIR,
) -> dict[str, list[list[str]]]:
    grammar = read_grammar(paths, meta_parser_dir)
    check_references(grammar)
    if not grammar.parser_rule_order:
        msg = "the grammar has no parser rule"
        raise G4Error(msg)
    if start is None:
        start = grammar.parser_rule_order[0]
    if start not in grammar.rules or grammar.rules[start].is_lexer:
        msg = f"the start rule {start!r} is not a parser rule"
        raise G4Error(msg)
    rules = JsonWriter(grammar).write(start)
    return clean(rules)


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("grammars", nargs="+", type=Path, help="the .g4 files (lexer and parser)")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        required=True,
        help="the JSON grammar file to write",
    )
    parser.add_argument("-s", "--start", help="the start rule (default: the first parser rule)")
    parser.add_argument(
        "--meta-dir",
        type=Path,
        default=DEFAULT_META_PARSER_DIR,
        help="the folder of the generated meta-grammar parser (default: %(default)s)",
    )
    args = parser.parse_args(None if argv is None else list(argv))

    try:
        rules = convert(args.grammars, args.start, args.meta_dir.resolve())
    except (G4Error, OSError) as e:
        print(f"[-] {e}", file=sys.stderr)
        return 1
    args.output.write_text(json.dumps(rules, indent=1) + "\n")
    print(f"[+] Wrote {len(rules)} rules to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
