#!/usr/bin/env python3
#
# american fuzzy lop++ - custom code formatter
# --------------------------------------------
#
# Written and maintained by Andrea Fioraldi <andreafioraldi@gmail.com>
#
# Copyright 2015, 2016, 2017 Google Inc. All rights reserved.
# Copyright 2019-2023 AFLplusplus Project. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# This is the AFL++ formatter with these changes for the Grammar Mutator:
#
# - It requires one clang-format major version on every path, because other
#   major versions give different output.
# - It also looks for clang-format in the .venv folder (see `make format-deps`).
# - It also runs on systems other than Linux. The pinned clang-format version
#   gives the same output on all systems.
# - It does not add a blank line at the end of each file.
# - It stops if clang-format fails, so that it does not write an empty file.
#

from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

CLANG_FORMAT_MAJOR = os.getenv("LLVM_VERSION", "23")

ROOT_DIR = Path(__file__).resolve().parent
VENV_BIN = ROOT_DIR / ".venv" / "bin" / "clang-format"
FORMAT_CACHE_DIR = ROOT_DIR / ".format-cache"


def clang_format_version(binary: str) -> str | None:
    """Return the output of `binary --version`, or None if it cannot run."""
    path = shutil.which(binary)
    if path is None:
        return None
    try:
        output = subprocess.check_output([path, "--version"])
    except (OSError, subprocess.SubprocessError):
        return None
    return output.decode("utf-8", "replace")


def has_major_version(version: str | None) -> bool:
    return version is not None and f"version {CLANG_FORMAT_MAJOR}." in version


def find_clang_format() -> str:
    """Find a clang-format binary with the required major version."""
    override = os.getenv("CLANG_FORMAT_BIN")
    if override:
        if not has_major_version(clang_format_version(override)):
            print(
                f"[!] CLANG_FORMAT_BIN={override} is not clang-format "
                f"{CLANG_FORMAT_MAJOR}. Aborted."
            )
            sys.exit(1)
        return override

    candidates = (str(VENV_BIN), f"clang-format-{CLANG_FORMAT_MAJOR}", "clang-format")
    for candidate in candidates:
        if has_major_version(clang_format_version(candidate)):
            return candidate

    print(f"[!] clang-format {CLANG_FORMAT_MAJOR} is needed. Aborted.")
    print("Run `make format-deps` to install it into .venv.")
    sys.exit(1)


def read_column_limit() -> int:
    """Read ColumnLimit from .clang-format. The default is 80."""
    column_limit = 80
    for config_line in (ROOT_DIR / ".clang-format").read_text().split("\n"):
        key, _, value = config_line.partition(":")
        if key.strip() == "ColumnLimit":
            column_limit = int(value.strip())
    return column_limit


CLANG_FORMAT_BIN = find_clang_format()
CLANG_FORMAT_VERSION = subprocess.check_output([CLANG_FORMAT_BIN, "--version"])
COLUMN_LIMIT = read_column_limit()


def custom_format(filename: str) -> str:
    result = subprocess.run(
        [CLANG_FORMAT_BIN, filename], stdout=subprocess.PIPE, check=False
    )
    if result.returncode != 0:
        print(f"[!] clang-format failed on {filename}. Aborted.")
        sys.exit(1)

    lines = result.stdout.decode("utf-8").split("\n")
    # The output of clang-format ends with a newline. Do not add a blank line
    # for the empty string after it.
    if lines and lines[-1] == "":
        lines.pop()

    in_define = False
    last_line: str | None = None
    out = ""

    for line in lines:
        stripped = line.lstrip()
        define_start = stripped.startswith("#") and stripped[1:].lstrip().startswith(
            "define"
        )

        new_line = line
        if (
            "/*" in new_line
            and not new_line.strip().startswith("/*")
            and new_line.endswith("*/")
            and len(new_line) < (COLUMN_LIMIT - 2)
        ):
            cmt_start = new_line.rfind("/*")
            new_line = (
                new_line[:cmt_start]
                + " " * (COLUMN_LIMIT - 2 - len(new_line))
                + new_line[cmt_start:]
            )

        define_padding = 0
        if last_line is not None and in_define and last_line.endswith("\\"):
            last_line = last_line[:-1]
            define_padding = max(0, len(last_line[last_line.rfind("\n") + 1 :]))

        # Add a blank line after an opening brace, after a closing brace, and
        # before a closing brace. In a macro, continue the blank line with "\".
        needs_blank_line = last_line is not None and (
            (last_line.strip().endswith("{") and new_line.strip() != "")
            or (last_line.strip().startswith("}") and new_line.strip() != "")
            or (new_line.strip().startswith("}") and last_line.strip() != "")
        )
        if needs_blank_line:
            continuation = " " * define_padding + "\\" if in_define else ""
            new_line = continuation + "\n" + new_line

        in_define = (define_start or in_define) and new_line.endswith("\\")

        out += new_line + "\n"
        last_line = new_line

    return out


def hash_code_and_formatter(code: str) -> str:
    hasher = hashlib.sha256()
    hasher.update(code.encode())
    hasher.update(CLANG_FORMAT_VERSION)
    hasher.update(Path(__file__).read_bytes())
    return hasher.hexdigest()


def custom_format_cached(filename: str) -> str:
    FORMAT_CACHE_DIR.mkdir(parents=True, exist_ok=True)
    filename_hash = hashlib.sha256(str(Path(filename).resolve()).encode()).hexdigest()
    cache_file = FORMAT_CACHE_DIR / filename_hash

    if cache_file.exists():
        code = Path(filename).read_text()
        if cache_file.read_text() == hash_code_and_formatter(code):
            return code

    code = custom_format(filename)
    cache_file.write_text(hash_code_and_formatter(code))
    return code


def main(args: list[str]) -> None:
    if not args:
        print("Usage: ./.custom-format.py [-i] <filename>")
        print()
        print(" The -i option, if specified, let the script to modify in-place")
        print(" the source files. By default the results are written to stdout.")
        print()
        sys.exit(1)

    in_place = False
    if args[0] == "-i":
        in_place = True
        args = args[1:]

    for filename in args:
        code = custom_format_cached(filename)
        if in_place:
            Path(filename).write_text(code)
        else:
            print(code, end="")


if __name__ == "__main__":
    main(sys.argv[1:])
