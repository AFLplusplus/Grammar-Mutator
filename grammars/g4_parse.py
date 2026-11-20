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
"""Parse test cases with an ANTLR Python parser and make generator trees.

g4_to_json.py writes a JSON grammar and a parse map. ANTLR writes a Python
lexer and parser from the same .g4 files. This file parses a test case with
that parser, and then changes the ANTLR parse tree into a tree of the JSON
grammar (the tree that the mutator reads):

- Each parse tree node of a parser rule gets the JSON key of that rule. Its
  children must match one alternative of the key, through the helper keys.
- A token gets one leaf with the exact token text. A separator leaf gets the
  text between its token and the next token (whitespace, comments, and
  skipped tokens). The text before the first token goes to the lead leaf.

So the leaves of the tree give the exact text of the test case.
"""

# The generated ANTLR parser has no type information, so the parse tree
# contexts have the type Any.
# ruff: noqa: ANN401

from __future__ import annotations

import importlib.util
import sys
import threading
from typing import TYPE_CHECKING, Any, NamedTuple, Tuple

from f1_common import LimitFuzzer
from g4_to_json import START_KEY, SyntaxErrorCounter
from g4_tree import RULE_ID_UNKNOWN, TERM_ID, Node, text

if TYPE_CHECKING:
    from pathlib import Path

# The ANTLR runtime parses with recursion, one level (or more) for each level
# of the input. A thread with a large stack and a high recursion limit lets it
# parse deep input.
PARSER_STACK_SIZE = 512 * 1024 * 1024
PARSER_RECURSION_LIMIT = 200000

# The events of a match, which _build_nodes changes into nodes
_OPEN = 0  # (_OPEN, key, alternative): start a node of key
_CLOSE = 1  # (_CLOSE, count): end count nodes
_LEAF = 2  # (_LEAF, text): a leaf
_TOKEN = 3  # (_TOKEN, key, child): a token node for child
_CTX = 4  # (_CTX, child): a rule node for child (matched later)

# A frame of the match stack: a key, an alternative of it (the JSON index),
# the index of the next symbol, and the number of nodes to end with it.
Frame = Tuple[str, int, int, int]
# A choice point: the frames of the callers, the child position, the helper
# key to enter, and the number of nodes to end with its frame.
Choice = Tuple[Tuple[Frame, ...], int, str, int]


class ParseError(Exception):
    """A test case that does not give a tree. reason tells why."""

    def __init__(self, reason: str) -> None:
        super().__init__(reason)
        self.reason = reason


class Kid(NamedTuple):
    """A child of a parse tree node: a rule node or a token."""

    key: str | None = None  # the JSON key of a rule node
    ctx: Any = None  # the parse tree node of a rule node
    type_name: str | None = None  # the token type name of a token
    text: str | None = None  # the text of a token
    hidden: str = ""  # the text after a token, up to the next token


def node_ids(grammar: dict[str, list[list[str]]]) -> dict[str, int]:
    """Return the node id of each key: its position in the key order + 1."""
    return {k: i + 1 for i, k in enumerate(grammar)}


def alternative_order(grammar: dict[str, list[list[str]]]) -> dict[str, list[int]]:
    """Return the JSON alternative indexes of each key in F1 order.

    f1_c_gen.py sorts the alternatives in this order, and the rule_id of a
    node is the position of its alternative in this list.
    """
    cost = LimitFuzzer(grammar).cost
    order = {}
    for k, alternatives in grammar.items():
        # The alternatives of a key are unique (g4_to_json.clean).
        index = {tuple(alternative): i for i, alternative in enumerate(alternatives)}
        order[k] = [index[tuple(rule)] for _cost, rule in cost[k]]
    return order


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
                    raise ValueError(msg)
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
        raise ValueError(msg)


def load_class(parser_dir: Path, module_name: str) -> Any:
    """Import the class module_name from the module module_name.py in parser_dir.

    The folder goes on sys.path, and the module goes into sys.modules with
    its name, because the base classes of a grammar with action code import
    the generated modules by name. A module with the same name from an
    earlier call is replaced.
    """
    path = parser_dir / f"{module_name}.py"
    spec = importlib.util.spec_from_file_location(module_name, path)
    if spec is None or spec.loader is None:
        msg = f"cannot import {path}"
        raise ImportError(msg)
    if str(parser_dir) not in sys.path:
        sys.path.insert(0, str(parser_dir))
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return getattr(module, module_name)


class TreeBuilder:
    """Change test cases into trees of a JSON grammar from g4_to_json.py."""

    def __init__(self, grammar: dict[str, list[list[str]]], parse_map: dict[str, Any], parser_dir: Path) -> None:
        check_helpers(grammar, parse_map)
        self.grammar = grammar
        self.kinds: dict[str, list[list[str]]] = parse_map["kinds"]
        self.token_keys: dict[str, str] = parse_map["token_keys"]
        self.parser_rules: dict[str, str] = parse_map["parser_rules"]
        self.start_rule: str = parse_map["start_rule"]
        self.ids = node_ids(grammar)
        self.order = alternative_order(grammar)
        self.rule_ids = {k: {alt: i for i, alt in enumerate(order)} for k, order in self.order.items()}
        self.lexer_class = load_class(parser_dir, parse_map["lexer_module"])
        self.parser_class = load_class(parser_dir, parse_map["parser_module"])

    # -- parsing -------------------------------------------------------------

    def build(self, data: bytes) -> Node:
        """Return the tree of a test case. Raise ParseError if there is none."""
        try:
            source = data.decode("utf-8")
        except UnicodeDecodeError:
            raise ParseError("not UTF-8") from None
        tree = self._parse_in_thread(source)

        from antlr4 import Token  # noqa: PLC0415
        from antlr4.tree.Tree import TerminalNode  # noqa: PLC0415

        # The visible tokens, in input order. The text between them comes from
        # the input, so it also has the skipped tokens.
        tokens = []
        stack = [tree]
        while stack:
            node = stack.pop()
            if isinstance(node, TerminalNode):
                if node.symbol.type != Token.EOF:
                    tokens.append(node.symbol)
            else:
                stack.extend(reversed(node.children or []))
        leading = source[: tokens[0].start] if tokens else source
        ends = [t.start for t in tokens[1:]] + [len(source)]
        hidden = {t.tokenIndex: source[t.stop + 1 : end] for t, end in zip(tokens, ends)}

        def kids_of(ctx: Any) -> list[Kid]:
            kids = []
            for child in ctx.children or []:
                if not isinstance(child, TerminalNode):
                    rule = self.parser_class.ruleNames[child.getRuleIndex()]
                    kids.append(Kid(key=self.parser_rules.get(rule), ctx=child))
                elif child.symbol.type != Token.EOF:
                    t = child.symbol
                    names = self.parser_class.symbolicNames
                    name = names[t.type] if t.type < len(names) else ""
                    kids.append(Kid(type_name=name, text=source[t.start : t.stop + 1], hidden=hidden[t.tokenIndex]))
            return kids

        root = Node(self.ids[START_KEY])
        start_kids = [Kid(key=self.parser_rules.get(self.start_rule), ctx=tree)]
        todo: list[tuple[str, list[Kid], Node]] = [(START_KEY, start_kids, root)]
        while todo:
            key, kids, node = todo.pop()
            events = self._match(key, kids, leading)
            if events is None:
                raise ParseError("no match")
            for kid, child in self._build_nodes(events, node, kids):
                todo.append((kid.key or "", kids_of(kid.ctx), child))
        # The leaves give the input text by construction. Check it, so that
        # a tree with other text never goes to the fuzzer.
        if text(root) != source:
            raise ParseError("no match")
        return root

    def _parse_in_thread(self, source: str) -> Any:
        """Parse source with ANTLR in a thread with a large stack."""
        result: list[Any] = []

        def parse() -> None:
            try:
                result.append(self._parse(source))
            except Exception as e:  # any failure in the ANTLR runtime
                result.append(e)

        old_limit = sys.getrecursionlimit()
        old_size = threading.stack_size(PARSER_STACK_SIZE)
        try:
            sys.setrecursionlimit(max(old_limit, PARSER_RECURSION_LIMIT))
            thread = threading.Thread(target=parse)
            thread.start()
            thread.join()
        finally:
            threading.stack_size(old_size)
            sys.setrecursionlimit(old_limit)
        outcome = result[0] if result else None
        if isinstance(outcome, ParseError):
            raise outcome
        if outcome is None or isinstance(outcome, Exception):
            raise ParseError("parser limit")
        return outcome

    def _parse(self, source: str) -> Any:
        from antlr4 import CommonTokenStream, InputStream  # noqa: PLC0415

        lexer = self.lexer_class(InputStream(source))
        parser = self.parser_class(CommonTokenStream(lexer))
        errors = SyntaxErrorCounter("input")
        for recognizer in (lexer, parser):
            recognizer.removeErrorListeners()
            recognizer.addErrorListener(errors)
        # The Python target adds "_" to a rule name that Python reserves.
        start = getattr(parser, self.start_rule, None) or getattr(parser, self.start_rule + "_")
        tree = start()
        if errors.errors:
            raise ParseError("syntax error")
        return tree

    # -- matching ------------------------------------------------------------

    def _match(self, key: str, kids: list[Kid], leading: str) -> list[tuple[Any, ...]] | None:
        """Match the children of a parse tree node against the alternatives of key.

        Return the events of the first match in F1 order, or None. The search
        is depth first, with a stack, not recursion. Each choice point enters
        a helper key. A choice point that comes back with the same callers at
        the same position failed before (or is an empty loop), so the search
        skips it. So each choice point runs at most once per child position.
        """
        events: list[tuple[Any, ...]] = []
        visited: set[tuple[Any, ...]] = set()
        # Each entry: the choice point, the next alternative to try, and the
        # number of events before the choice point.
        choices: list[list[Any]] = []
        point: Choice | bool = ((), 0, key, 1)
        while point is not True:
            if point is not False:
                callers, i, helper, _closes = point
                memo = (tuple(frame[:3] for frame in callers), helper, i)
                if memo not in visited and helper in self.kinds:
                    visited.add(memo)
                    choices.append([point, 0, len(events)])
            # Try the next alternative of the newest choice point.
            while choices and choices[-1][1] == len(self.order[choices[-1][0][2]]):
                choices.pop()
            if not choices:
                return None
            entry = choices[-1]
            (callers, i, helper, closes), index, mark = entry
            entry[1] = index + 1
            del events[mark:]
            alternative = self.order[helper][index]
            events.append((_OPEN, helper, alternative))
            point = self._run([*callers, (helper, alternative, 0, closes)], i, kids, leading, events)
        return events

    def _run(  # noqa: PLR0911, PLR0912
        self, stack: list[Frame], i: int, kids: list[Kid], leading: str, events: list[tuple[Any, ...]]
    ) -> Choice | bool:
        """Match symbols from a stack of frames until the next choice point.

        Return the choice point, True at the end of a match, or False.
        """
        while stack:
            key, alternative, sym, closes = stack[-1]
            symbols = self.grammar[key][alternative]
            if sym == len(symbols):
                stack.pop()
                events.append((_CLOSE, closes))
                continue
            symbol_kinds = self.kinds[key][alternative]
            symbol, kind = symbols[sym], symbol_kinds[sym]
            stack[-1] = (key, alternative, sym + 1, closes)
            kid = kids[i] if i < len(kids) else Kid()
            if symbol in self.token_keys:
                # A token key is one token. Do not look into its alternatives.
                if kid.type_name != self.token_keys[symbol]:
                    return False
                events.append((_TOKEN, symbol, i))
                i += 1
            elif kind == "rule":
                if kid.key != symbol:
                    return False
                events.append((_CTX, i))
                i += 1
            elif kind == "literal":
                end = sym + 1
                while end < len(symbols) and symbol_kinds[end] == "literal_cont":
                    end += 1
                if kid.text != "".join(symbols[sym:end]):
                    return False
                events.extend((_LEAF, piece) for piece in symbols[sym:end])
                stack[-1] = (key, alternative, end, closes)
                i += 1
            elif kind == "sep":
                # The text after the token before the separator
                if i == 0 or kids[i - 1].type_name is None:
                    return False
                events.append((_LEAF, kids[i - 1].hidden))
            elif kind == "lead":
                events.append((_LEAF, leading))
            elif kind == "helper":
                if sym + 1 == len(symbols):
                    # The last symbol: the frame of key ends with the helper.
                    stack.pop()
                    return (tuple(stack), i, symbol, closes + 1)
                return (tuple(stack), i, symbol, 1)
            else:
                return False
        return i == len(kids)

    def _build_nodes(self, events: list[tuple[Any, ...]], target: Node, kids: list[Kid]) -> list[tuple[Kid, Node]]:
        """Add the nodes of a match to target (the node of the first event).

        Return the rule children and their new nodes, which the caller must
        match next.
        """
        rule_kids = []
        stack: list[Node] = []
        for event in events:
            tag = event[0]
            if tag == _OPEN:
                rule_id = self.rule_ids[event[1]][event[2]]
                if stack:
                    node = Node(self.ids[event[1]], rule_id)
                    stack[-1].children.append(node)
                else:
                    node = target
                    node.rule_id = rule_id
                stack.append(node)
            elif tag == _CLOSE:
                del stack[-event[1] :]
            elif tag == _LEAF:
                stack[-1].children.append(Node(TERM_ID, 0, event[1]))
            elif tag == _TOKEN:
                leaf = Node(TERM_ID, 0, kids[event[2]].text or "")
                stack[-1].children.append(Node(self.ids[event[1]], RULE_ID_UNKNOWN, "", [leaf]))
            else:
                kid = kids[event[1]]
                child = Node(self.ids[kid.key or ""])
                stack[-1].children.append(child)
                rule_kids.append((kid, child))
        return rule_kids
