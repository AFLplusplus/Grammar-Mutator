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
"""Write the C++ parse tables of a .g4 grammar.

g4_to_json.py writes a JSON grammar and a parse map. This script writes
g4_grammar.cpp from them. The file defines `const g4::Grammar NS::grammar`
(see lib/antlr4_shim/g4_matcher.h): the keys, their alternatives in F1 order,
the symbol kinds, the token type names, the parser rule names, and the
functions that make the ANTLR lexer and parser. The C++ matcher reads only
these tables.

The script runs with the system python3. It needs only the standard library
and f1_common.py.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any, Dict, List, NamedTuple, Tuple

from f1_common import LimitFuzzer

# The key of the start symbol. It is the same as g4_to_json.START_KEY. The
# script does not import g4_to_json, because that module needs the antlr4
# package, and the system python3 does not have it.
START_KEY = "<START>"
# The parse map format that g4_to_json.py writes
PARSE_MAP_VERSION = 1

# The reserved words of the ANTLR 4.13.2 C++ target: the set in
# org/antlr/v4/codegen/target/CppTarget.class in antlr-4.13.2-complete.jar.
# The target adds "_" to a rule name in this set (Target.escapeIfNeeded).
CPP_RESERVED_WORDS = frozenset(
    """
    alignas alignof and and_eq asm auto bitand bitor bool break case catch char
    char16_t char32_t class compl concept const constexpr const_cast continue
    decltype default delete do double dynamic_cast else enum explicit export
    extern false float for friend goto if inline int long mutable namespace new
    noexcept not not_eq nullptr NULL operator or or_eq private protected public
    register reinterpret_cast requires return short signed sizeof static
    static_assert static_cast struct switch template this thread_local throw
    true try typedef typeid typename union unsigned using virtual void volatile
    wchar_t while xor xor_eq rule parserRule reset
    """.split()  # noqa: SIM905 (a word list is easier to read)
)

# The parse map kinds and their names in g4::Kind
KINDS = {
    "rule": "Rule",
    "token": "Token",
    "literal": "Literal",
    "literal_cont": "LiteralCont",
    "sep": "Sep",
    "lead": "Lead",
    "helper": "Helper",
}
# The kinds whose symbol is a key. The symbol of the other kinds is text.
KEY_KINDS = ("rule", "token", "helper")

# A symbol in the tables: the g4::Kind name, the key index, and the text
TableSymbol = Tuple[str, int, str]
# typing.Dict, not dict[...]: this alias runs at import time on Python 3.8.
Rules = Dict[str, List[List[str]]]


class GrammarError(ValueError):
    """A JSON grammar or a parse map that the matcher cannot use."""


class KeyTable(NamedTuple):
    """The table of one key."""

    name: str
    # The alternatives in F1 order. The position is the rule_id.
    alternatives: list[list[TableSymbol]]
    # The ANTLR token type name of a token key, or None
    token_type: str | None


def check_helpers(grammar: dict[str, list[list[str]]], parse_map: dict[str, Any]) -> None:
    """Make sure that the helper keys give a regular language for each key.

    The matcher needs this to stop. A key can refer to itself only as the last
    symbol of an alternative (the helper of `*` or `+`). The other references
    between helper keys must not make a cycle.
    """
    kinds = parse_map["kinds"]
    edges: dict[str, set[str]] = {k: set() for k in kinds}
    for k, alternatives in kinds.items():
        for alternative, symbol_kinds in zip(grammar[k], alternatives):
            for i, (symbol, kind) in enumerate(zip(alternative, symbol_kinds)):
                if kind != "helper":
                    continue
                if symbol != k:
                    edges[k].add(symbol)
                elif i != len(alternative) - 1:
                    msg = f"the helper key {k} refers to itself before the end of an alternative"
                    raise GrammarError(msg)
    # Remove the keys without references until no key is left (Kahn).
    refs = dict.fromkeys(edges, 0)
    for targets in edges.values():
        for target in targets:
            refs[target] = refs.get(target, 0) + 1
    todo = [k for k, count in refs.items() if count == 0]
    while todo:
        for target in edges.get(todo.pop(), ()):
            refs[target] -= 1
            if refs[target] == 0:
                todo.append(target)
    cycle = sorted(k for k, count in refs.items() if count > 0)
    if cycle:
        msg = f"the helper keys make a cycle: {', '.join(cycle)}"
        raise GrammarError(msg)


def cpp_method_name(rule: str) -> str:
    """Return the name of the method that the ANTLR C++ target makes for a rule."""
    return rule + "_" if rule in CPP_RESERVED_WORDS else rule


def cpp_string(text: str) -> str:
    """Return a braced std::string_view initializer for the UTF-8 bytes of text.

    Each byte that is not printable ASCII becomes a three-digit octal escape,
    so the text can hold any byte (also NUL). A lone surrogate keeps its
    bytes (surrogatepass); such bytes never match valid UTF-8 input.
    """
    data = text.encode("utf-8", "surrogatepass")
    body = []
    for byte in data:
        char = chr(byte)
        if char in '"\\?':
            body.append("\\" + char)
        elif 0x20 <= byte <= 0x7E:  # noqa: PLR2004
            body.append(char)
        else:
            body.append(f"\\{byte:03o}")
    return f'{{"{"".join(body)}", {len(data)}}}'


def f1_order(grammar: dict[str, list[list[str]]]) -> dict[str, list[int]]:
    """Return the JSON alternative indexes of each key in F1 order.

    f1_c_gen.py sorts the alternatives in this order, and the rule_id of a
    node is the position of its alternative in this list. An alternative
    without a finite cost is not in the list.
    """
    cost = LimitFuzzer(grammar).cost
    order = {}
    for k, alternatives in grammar.items():
        # The alternatives of a key are unique (g4_to_json.clean).
        index = {tuple(alternative): i for i, alternative in enumerate(alternatives)}
        order[k] = [index[tuple(rule)] for _cost, rule in cost[k]]
    return order


def table_symbols(
    key: str, symbols: list[str], kinds: list[str], ids: dict[str, int], token_keys: dict[str, str]
) -> list[TableSymbol]:
    """Return the table symbols of one alternative of a key."""
    if len(symbols) != len(kinds):
        msg = f"the grammar and the parse map do not agree: {key} has an alternative with other symbols"
        raise GrammarError(msg)
    result: list[TableSymbol] = []
    for i, (symbol, kind) in enumerate(zip(symbols, kinds)):
        if kind not in KINDS:
            msg = f"the key {key} has a symbol of kind {kind!r}"
            raise GrammarError(msg)
        if kind == "literal_cont" and (i == 0 or kinds[i - 1] not in ("literal", "literal_cont")):
            msg = f"the key {key} has a literal_cont symbol that does not follow a literal symbol"
            raise GrammarError(msg)
        if kind == "token" and symbol not in token_keys:
            msg = f"the token key {symbol} has no token type name in the parse map"
            raise GrammarError(msg)
        if symbol in token_keys:
            # The matcher checks a token key first, for each kind.
            result.append((KINDS["token"], ids[symbol], ""))
        elif kind in KEY_KINDS:
            if symbol not in ids:
                msg = f"the symbol {symbol} of the key {key} is not a key"
                raise GrammarError(msg)
            result.append((KINDS[kind], ids[symbol], ""))
        else:
            result.append((KINDS[kind], 0, symbol))
    return result


def build_tables(grammar: dict[str, list[list[str]]], parse_map: dict[str, Any]) -> list[KeyTable]:
    """Check the grammar and the parse map, and return the table of each key.

    The keys are in JSON order: the node id of a key is its index + 1. A key
    that the parse map does not describe gets no alternatives, because the
    matcher never enters it. Raise GrammarError if the input is not valid.
    """
    if not isinstance(grammar, dict) or not isinstance(parse_map, dict):
        msg = "the grammar and the parse map must be JSON objects"
        raise GrammarError(msg)
    version = parse_map.get("version")
    if version != PARSE_MAP_VERSION:
        msg = f"the parse map has version {version!r}, not {PARSE_MAP_VERSION}"
        raise GrammarError(msg)
    try:
        kinds: dict[str, list[list[str]]] = parse_map["kinds"]
        token_keys: dict[str, str] = parse_map["token_keys"]
        if START_KEY not in kinds or any(k not in grammar or len(grammar[k]) != len(kinds[k]) for k in kinds):
            msg = "the grammar and the parse map do not agree: the keys or the alternatives are not the same"
            raise GrammarError(msg)
        check_helpers(grammar, parse_map)
        ids = {k: i for i, k in enumerate(grammar)}
        order = f1_order(grammar)
        tables = []
        for k in grammar:
            alternatives = []
            if k in kinds:
                for index in order[k]:
                    alternatives.append(table_symbols(k, grammar[k][index], kinds[k][index], ids, token_keys))
            tables.append(KeyTable(k, alternatives, token_keys.get(k)))
    except (KeyError, TypeError) as e:
        msg = f"the grammar and the parse map do not agree: {e!r}"
        raise GrammarError(msg) from e
    for k in token_keys:
        if k not in ids:
            msg = f"the token key {k} is not a key"
            raise GrammarError(msg)
    return tables


def parser_rule_table(grammar: dict[str, list[list[str]]], parse_map: dict[str, Any]) -> list[tuple[str, int]]:
    """Return the parser rule names and the index of their keys."""
    ids = {k: i for i, k in enumerate(grammar)}
    try:
        parser_rules: dict[str, str] = parse_map["parser_rules"]
        start_rule: str = parse_map["start_rule"]
        rules = []
        for name, k in parser_rules.items():
            if k not in parse_map["kinds"]:
                msg = f"the parser rule {name} has the key {k}, which the parse map does not describe"
                raise GrammarError(msg)
            rules.append((name, ids[k]))
        if start_rule not in parser_rules:
            msg = f"the start rule {start_rule!r} is not in the parser rules of the parse map"
            raise GrammarError(msg)
    except (KeyError, TypeError) as e:
        msg = f"the grammar and the parse map do not agree: {e!r}"
        raise GrammarError(msg) from e
    for module in ("lexer_module", "parser_module"):
        if not isinstance(parse_map.get(module), str):
            msg = f"the parse map has no {module.replace('_module', '')} module"
            raise GrammarError(msg)
    return rules


def generate(
    grammar: dict[str, list[list[str]]],
    parse_map: dict[str, Any],
    namespace: str,
    lexer_header: str | None = None,
    parser_header: str | None = None,
) -> str:
    """Return the text of g4_grammar.cpp. Raise GrammarError for bad input."""
    tables = build_tables(grammar, parse_map)
    rules = parser_rule_table(grammar, parse_map)
    lexer_class: str = parse_map["lexer_module"]
    parser_class: str = parse_map["parser_module"]
    lexer_header = lexer_header or f"{lexer_class}.h"
    parser_header = parser_header or f"{parser_class}.h"
    method = cpp_method_name(parse_map["start_rule"])

    # The names in the file start with "g4_", so that they do not hide the
    # generated ANTLR classes in the same namespace.
    out = [
        "// Generated by grammars/g4_c_gen.py. Do not edit.",
        "",
        "#include <memory>",
        "",
        '#include "g4_matcher.h"',
        f"#include {json.dumps(lexer_header)}",
        f"#include {json.dumps(parser_header)}",
        "",
        f"namespace {namespace} {{",
        "",
        "namespace {",
        "",
    ]
    for index, table in enumerate(tables):
        entries = []
        for rule_id, alternative in enumerate(table.alternatives):
            if not alternative:
                entries.append("{nullptr, 0}")
                continue
            name = f"g4_k{index}_r{rule_id}"
            out.append(f"const g4::Symbol {name}[] = {{")
            out.extend(f"    {{g4::Kind::{kind}, {key}, {cpp_string(text)}}}," for kind, key, text in alternative)
            out.append("};")
            entries.append(f"{{{name}, {len(alternative)}}}")
        if entries:
            out.append(f"const g4::Alternative g4_k{index}[] = {{{', '.join(entries)}}};")
    out.append("const g4::Key g4_keys[] = {")
    for index, table in enumerate(tables):
        alternatives = f"g4_k{index}" if table.alternatives else "nullptr"
        token_type = cpp_string(table.token_type or "")
        out.append(f"    {{{cpp_string(table.name)}, {alternatives}, {len(table.alternatives)}, {token_type}}},")
    out.append("};")
    out.append("const g4::ParserRule g4_parser_rules[] = {")
    out.extend(f"    {{{cpp_string(name)}, {key}}}," for name, key in rules)
    out.append("};")
    out += [
        "",
        "std::unique_ptr<antlr4::Lexer> g4_make_lexer(antlr4::CharStream *input) {",
        f"  return std::make_unique<{lexer_class}>(input);",
        "}",
        "",
        "std::unique_ptr<antlr4::Parser> g4_make_parser(antlr4::TokenStream *tokens) {",
        f"  return std::make_unique<{parser_class}>(tokens);",
        "}",
        "",
        "antlr4::ParserRuleContext *g4_parse_start(antlr4::Parser &parser) {",
        f"  return static_cast<{parser_class} &>(parser).{method}();",
        "}",
        "",
        "}  // namespace",
        "",
        "extern const g4::Grammar grammar;",
        "const g4::Grammar grammar = {",
        f"    g4_keys, {len(tables)},",
        f"    g4_parser_rules, {len(rules)},",
        f"    {[t.name for t in tables].index(START_KEY)},",
        f"    {cpp_string(parse_map['start_rule'])},",
        "    g4_make_lexer,",
        "    g4_make_parser,",
        "    g4_parse_start,",
        "};",
        "",
        f"}}  // namespace {namespace}",
        "",
    ]
    return "\n".join(out)


def check_namespace(namespace: str) -> None:
    """Make sure that namespace is a C++ identifier that is not reserved."""
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", namespace) or namespace in CPP_RESERVED_WORDS:
        msg = f"the namespace {namespace!r} is not a C++ identifier that you can use"
        raise GrammarError(msg)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--grammar", type=Path, required=True, help="the JSON grammar from g4_to_json.py")
    parser.add_argument("--parse-map", type=Path, required=True, help="the parse map from g4_to_json.py")
    parser.add_argument("--namespace", required=True, help="the C++ namespace (the same as ANTLR -package)")
    parser.add_argument("--lexer-header", help="the header of the lexer (default: <lexer_module>.h)")
    parser.add_argument("--parser-header", help="the header of the parser (default: <parser_module>.h)")
    parser.add_argument("-o", "--output-dir", type=Path, required=True, help="the folder for g4_grammar.cpp")
    args = parser.parse_args(argv)

    try:
        check_namespace(args.namespace)
        grammar = json.loads(args.grammar.read_text(encoding="utf-8"))
        parse_map = json.loads(args.parse_map.read_text(encoding="utf-8"))
        source = generate(grammar, parse_map, args.namespace, args.lexer_header, args.parser_header)
        output = args.output_dir / "g4_grammar.cpp"
        output.write_text(source, encoding="utf-8")
    except (GrammarError, OSError, ValueError) as e:
        print(f"[-] {e}", file=sys.stderr)
        return 1
    print(f"[+] Wrote the parse tables of {len(grammar)} keys to {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
