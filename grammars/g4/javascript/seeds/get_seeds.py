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
"""Turn the test suites of JavaScript engines into JavaScript seeds.

Usage: get_seeds.py fetch ...
       get_seeds.py filter ...
       get_seeds.py count ...
       get_seeds.py -h | --help

The subcommands are:

  fetch   Download the test suites and select the seed files.
  filter  Keep only the seeds that a reference engine runs in time.
  count   Count the seeds of each suite.
"""

from __future__ import annotations

import re

# A runtime call follows one of these characters.
_PREFIX_CHARS = frozenset("(,=;{}[!&|?:+-")
# A runtime call also follows one of these keywords.
_PREFIX_WORDS = frozenset(
    "return typeof void throw await case in of new delete".split()
)
_NATIVE = re.compile(r"%([A-Za-z_$][A-Za-z0-9_$]*)\(")
_TRAILING_WORD = re.compile(r"[A-Za-z_$][A-Za-z0-9_$]*\Z")


def _starts_call(out: str) -> bool:
    """Return True if a runtime call can follow the text `out`."""
    stripped = out.rstrip(" \t\r\f\v")
    if not stripped or stripped[-1] == "\n":
        return True
    if stripped[-1] in _PREFIX_CHARS:
        return True
    word = _TRAILING_WORD.search(stripped)
    return word is not None and word.group() in _PREFIX_WORDS


def rewrite_natives(text: str) -> str:
    """Change V8 runtime calls into valid JavaScript.

    `%Name(` becomes `__v8native("Name")(`. The scan skips string literals,
    template literals, and comments. It does not track regular expressions.
    """
    out: list[str] = []
    length = len(text)
    i = 0
    while i < length:
        ch = text[i]
        if ch in "'\"":
            j = i + 1
            while j < length and text[j] != ch:
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, length)
        elif ch == "`":
            j = i + 1
            while j < length and text[j] != "`":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, length)
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = length if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = length if j < 0 else j + 2
        elif ch == "%":
            match = _NATIVE.match(text, i)
            if match and _starts_call("".join(out)):
                out.append('__v8native("%s")(' % match.group(1))
                i = match.end()
                continue
            j = i + 1
        else:
            j = i + 1
        out.append(text[i:j])
        i = j
    return "".join(out)
