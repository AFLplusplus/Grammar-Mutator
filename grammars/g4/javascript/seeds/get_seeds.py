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

import argparse
import hashlib
import re
import shutil
import subprocess
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path

# A runtime call follows one of these characters.
_PREFIX_CHARS = frozenset("(,=;{}[!&|?:+-")
# A runtime call also follows one of these keywords.
_PREFIX_WORDS = frozenset(
    "return typeof void throw await case in of new delete".split()
)
_NATIVE = re.compile(r"%([A-Za-z_$][A-Za-z0-9_$]*)\(")
_WORD_CHARS = frozenset(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$"
)


def _starts_call(text: str, pos: int) -> bool:
    """Return True if a runtime call can start at `pos` in `text`."""
    end = pos
    while end > 0 and text[end - 1] in " \t\f\v":
        end -= 1
    if end == 0 or text[end - 1] in "\n\r":
        return True
    if text[end - 1] in _PREFIX_CHARS:
        return True
    start = end
    while start > 0 and text[start - 1] in _WORD_CHARS:
        start -= 1
    return text[start:end] in _PREFIX_WORDS


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
            if match and _starts_call(text, i):
                out.append('__v8native("%s")(' % match.group(1))
                i = match.end()
                continue
            j = i + 1
        else:
            j = i + 1
        out.append(text[i:j])
        i = j
    return "".join(out)


# AFL++ skips a file that is larger than this (MAX_FILE).
MAX_SEED_BYTES = 1048576
# The longest file name that common file systems allow.
MAX_NAME_BYTES = 255


@dataclass(frozen=True)
class Suite:
    """One engine test suite.

    `root` is the repository folder that names are relative to.
    `folders` are the folders below `root` that hold seeds.
    """

    name: str
    url: str
    commit: str
    root: str
    folders: tuple[str, ...]


SUITES: tuple[Suite, ...] = (
    Suite(
        "test262",
        "https://github.com/tc39/test262",
        # The head of main on 2026-11-27.
        "4e4505c343d64b3c25da2338f2594504b0728594",
        "test",
        ("annexB", "built-ins", "intl402", "language", "staging"),
    ),
    Suite(
        "mjsunit",
        "https://github.com/v8/v8",
        # The head of main on 2026-11-27.
        "1344a52aeaec26ca151586e9f8d08517e0997235",
        "test/mjsunit",
        (".",),
    ),
    Suite(
        "jstests",
        "https://github.com/WebKit/WebKit",
        # The head of main on 2026-11-27.
        "4940531db2fa181a710f9e204390628ab44e5183",
        "JSTests",
        ("stress", "es6", "ChakraCore", "mozilla", "regexp"),
    ),
)

# Entries of the form "<suite>/<rel>" that get the reason `slow-parse`.
EXCLUDE: frozenset[str] = frozenset()

_FRONT_MATTER = re.compile(r"/\*---(.*?)---\*/", re.DOTALL)
_PHASE = re.compile(
    r"^[ \t]*negative:[ \t]*\n(?:[ \t]+[^\n]*\n)*?[ \t]+phase:[ \t]*(\w+)",
    re.M,
)
_FLAGS_FLOW = re.compile(r"^[ \t]*flags:[ \t]*\[([^\]]*)\]", re.M)
_FLAGS_BLOCK = re.compile(
    r"^[ \t]*flags:[ \t]*\n((?:[ \t]+-[^\n]*\n?)+)", re.M
)


def _front_matter(text: str) -> str:
    match = _FRONT_MATTER.search(text)
    if not match:
        return ""
    # Use one line ending, so that CRLF files work too.
    return match.group(1).replace("\r\n", "\n").replace("\r", "\n") + "\n"


def _flags(text: str) -> list[str]:
    """Return the flags of the front matter of a test262 test."""
    meta = _front_matter(text)
    flow = _FLAGS_FLOW.search(meta)
    if flow:
        return [f.strip() for f in flow.group(1).split(",") if f.strip()]
    block = _FLAGS_BLOCK.search(meta)
    if block:
        return [
            line.strip()[1:].strip()
            for line in block.group(1).splitlines()
            if line.strip()
        ]
    return []


def test262_drop_reason(text: str, name: str) -> str | None:
    """Return why a test262 test is not a seed, or None to keep it."""
    if name.endswith("_FIXTURE.js"):
        return "fixture"
    phase = _PHASE.search(_front_matter(text))
    if phase:
        if phase.group(1) == "parse":
            return "negative-parse"
        if phase.group(1) == "resolution":
            return "negative-resolution"
    if "module" in _flags(text):
        return "module"
    return None


def test262_only_strict(text: str) -> bool:
    """Return True if the test runs only in strict mode."""
    return "onlyStrict" in _flags(text)


def drop_reason(suite: str, rel: str, data: bytes) -> str | None:
    """Return why a file is not a seed, or None to keep it.

    `rel` is relative to the suite root.
    """
    parts = rel.split("/")
    if suite == "mjsunit" and rel == "mjsunit.js":
        return "harness"
    if suite == "jstests":
        base = parts[-1]
        if rel.startswith("mozilla/") and base in ("shell.js", "browser.js"):
            return "harness"
        if "resources" in parts:
            return "resources"
    if len(data) > MAX_SEED_BYTES:
        return "too-large"
    if suite == "test262":
        text = data.decode("utf-8", errors="replace")
        return test262_drop_reason(text, parts[-1])
    return None


_NAME_CHARS = frozenset(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-"
)


def seed_name(suite: str, rel: str) -> str:
    """Return the seed file name of the file `rel` of `suite`."""
    raw = rel.encode("utf-8", errors="surrogateescape")
    safe = "".join(chr(b) if chr(b) in _NAME_CHARS else "_" for b in raw)
    name = "%s_%s" % (suite, safe)
    if len(name) > MAX_NAME_BYTES:
        digest = hashlib.sha1(raw).hexdigest()
        tail = "_%s.js" % digest[:8]
        name = name[: MAX_NAME_BYTES - len(tail)] + tail
    return name


def unique_names(names: list[str]) -> list[str]:
    """Make every name unique.

    A repeated name gets `_2`, `_3`, ... before `.js`. A name stays within
    255 bytes: the code cuts the stem before it adds the suffix.
    """
    seen: set[str] = set()
    counts: Counter = Counter()
    result: list[str] = []
    for name in names:
        candidate = name
        while candidate in seen:
            counts[name] += 1
            stem = name[:-3] if name.endswith(".js") else name
            ext = ".js" if name.endswith(".js") else ""
            tail = "_%d%s" % (counts[name] + 1, ext)
            candidate = stem[: MAX_NAME_BYTES - len(tail)] + tail
        seen.add(candidate)
        result.append(candidate)
    return result


@dataclass
class BuildSummary:
    """Counts of one seed build. Keys are suite names."""

    files: Counter
    seeds: Counter
    dropped: dict[str, Counter]


def _seed_bytes(suite: str, data: bytes) -> bytes:
    if suite == "test262":
        text = data.decode("utf-8", errors="replace")
        return (b'"use strict";\n' if test262_only_strict(text) else b"") + data
    if suite == "mjsunit":
        text = data.decode("utf-8", errors="surrogateescape")
        return rewrite_natives(text).encode("utf-8", errors="surrogateescape")
    return data


def _suite_files(root: Path, folders: tuple[str, ...]) -> list[str]:
    rels = set()
    for folder in folders:
        for path in (root / folder).rglob("*.js"):
            if path.is_file():
                rels.add(path.relative_to(root).as_posix())
    return sorted(rels)


def build_seeds(
    roots: dict[str, Path],
    seeds_dir: Path,
    exclude: frozenset[str] = frozenset(),
) -> BuildSummary:
    """Write the seeds of the suites in `roots` to `seeds_dir/<suite>/`."""
    if seeds_dir.exists() and any(seeds_dir.iterdir()):
        raise SystemExit("seeds folder is not empty: %s" % seeds_dir)
    folders = {s.name: s.folders for s in SUITES}
    summary = BuildSummary(Counter(), Counter(), {})
    for suite in sorted(roots):
        root = roots[suite]
        dropped: Counter = Counter()
        summary.dropped[suite] = dropped
        kept: list[tuple[str, bytes]] = []
        for rel in _suite_files(root, folders[suite]):
            summary.files[suite] += 1
            data = (root / rel).read_bytes()
            reason = drop_reason(suite, rel, data)
            if reason is None and "%s/%s" % (suite, rel) in exclude:
                reason = "slow-parse"
            if reason:
                dropped[reason] += 1
            else:
                kept.append((rel, _seed_bytes(suite, data)))
        out = seeds_dir / suite
        out.mkdir(parents=True, exist_ok=True)
        names = unique_names([seed_name(suite, rel) for rel, _ in kept])
        for name, (_, data) in zip(names, kept):
            (out / name).write_bytes(data)
        summary.seeds[suite] = len(kept)
    return summary


def write_summary(
    path: Path, suites: tuple[Suite, ...], summary: BuildSummary
) -> None:
    """Write the counts of a build to a text file."""
    lines: list[str] = []
    for suite in suites:
        lines.append("%s %s" % (suite.name, suite.commit))
        lines.append("  files: %d" % summary.files[suite.name])
        lines.append("  seeds: %d" % summary.seeds[suite.name])
        dropped = summary.dropped.get(suite.name, {})
        for reason, count in sorted(dropped.items()):
            lines.append("  dropped %s: %d" % (reason, count))
    path.write_text("\n".join(lines) + "\n")


# The name of the git program.
GIT_PROGRAM = "git"


def _git(
    work: Path, url: str, *args: str, capture: bool = False
) -> subprocess.CompletedProcess:
    """Run git in `work`. Stop with a short message if git cannot run."""
    try:
        return subprocess.run(
            [GIT_PROGRAM, "-C", str(work)] + list(args),
            capture_output=capture,
            text=True,
        )
    except OSError as err:
        raise SystemExit(
            "[-] Cannot run %s for %s: %s" % (GIT_PROGRAM, url, err)
        )


def _git_ok(work: Path, url: str, *args: str) -> None:
    """Run git in `work`. Stop with a short message if it fails."""
    if _git(work, url, *args).returncode != 0:
        raise SystemExit(
            "[-] git %s failed for %s" % (" ".join(args), url)
        )


def _head(work: Path, url: str) -> str:
    """Return the commit of HEAD in `work`, or "" if there is none."""
    if not (work / ".git").exists():
        return ""
    result = _git(work, url, "rev-parse", "HEAD", capture=True)
    return result.stdout.strip() if result.returncode == 0 else ""


def _is_checked_out(
    work: Path, url: str, commit: str, paths: list[str]
) -> bool:
    if _head(work, url) != commit:
        return False
    return all((work / path).exists() for path in paths)


def fetch_repo(url: str, commit: str, paths: list[str], work: Path) -> Path:
    """Check out `paths` of `commit` of the repository `url` in `work`.

    The call does nothing if `work` already has this commit and these paths.
    Else it fetches only this commit, without file contents, and checks out
    only `paths`. It returns `work`.
    """
    if _is_checked_out(work, url, commit, paths):
        return work
    work.mkdir(parents=True, exist_ok=True)
    if not (work / ".git").exists():
        _git_ok(work, url, "init", "-q")
    remotes = _git(work, url, "remote", capture=True).stdout.split()
    _git_ok(
        work,
        url,
        "remote",
        "set-url" if "origin" in remotes else "add",
        "origin",
        url,
    )
    _git_ok(work, url, "config", "core.sparseCheckout", "true")
    info = work / ".git" / "info"
    info.mkdir(exist_ok=True)
    (info / "sparse-checkout").write_text(
        "".join("/%s/\n" % path for path in paths)
    )
    _git_ok(
        work,
        url,
        "fetch",
        "-q",
        "--depth",
        "1",
        "--filter=blob:none",
        "origin",
        commit,
    )
    # Check the paths before the checkout, so that the message is the same
    # for every git version.
    missing = [
        path
        for path in paths
        if not _git(
            work, url, "ls-tree", "-d", "FETCH_HEAD", "--", path, capture=True
        ).stdout.strip()
    ]
    if missing:
        raise SystemExit(
            "[-] %s at %s: missing path %s" % (url, commit, ", ".join(missing))
        )
    _git_ok(work, url, "checkout", "-q", "--force", "FETCH_HEAD")
    head = _head(work, url)
    if head != commit:
        raise SystemExit(
            "[-] %s: HEAD is %s, not the pinned commit %s"
            % (url, head, commit)
        )
    return work


def _sparse_paths(suite: Suite) -> list[str]:
    """Return the repository paths that a suite needs."""
    return [
        suite.root if folder == "." else "%s/%s" % (suite.root, folder)
        for folder in suite.folders
    ]


def _fetch(dest: Path) -> int:
    roots: dict[str, Path] = {}
    for suite in SUITES:
        print("[*] fetching %s at %s" % (suite.name, suite.commit))
        work = dest / "src" / suite.name
        fetch_repo(suite.url, suite.commit, _sparse_paths(suite), work)
        roots[suite.name] = work / suite.root
    summary = build_seeds(roots, dest / "seeds", EXCLUDE)
    shims = Path(__file__).resolve().parent / "shims.js"
    if shims.exists():
        shutil.copyfile(str(shims), str(dest / "shims.js"))
    write_summary(dest / "summary.txt", SUITES, summary)
    print((dest / "summary.txt").read_text(), end="")
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Turn engine test suites into JavaScript seeds.")
    sub = parser.add_subparsers(dest="command", required=True)
    fetch = sub.add_parser("fetch", help="download the suites, select seeds")
    fetch.add_argument("dest", type=Path, help="working folder")
    args = parser.parse_args(argv)
    # Each later subcommand adds a branch here.
    return _fetch(args.dest)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
