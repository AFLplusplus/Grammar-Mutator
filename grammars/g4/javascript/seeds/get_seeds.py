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
import os
import re
import shlex
import shutil
import signal
import subprocess
import sys
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path

# A runtime call follows one of these characters. After `>` (also in `=>`)
# a `%` cannot be the modulo operator.
_PREFIX_CHARS = frozenset("(,=;{}[!&|?:+->")
# A runtime call also follows one of these keywords.
_PREFIX_WORDS = frozenset(
    "return typeof void throw await case in of new delete else do".split()
)
# A `)` is a prefix only if it closes the `(` after one of these keywords.
_CONTROL_WORDS = frozenset(("if", "while", "for"))
# White space between a prefix and a runtime call: the WhiteSpace set of
# JavaScript. It has U+FEFF (a byte order mark) and the Zs characters, such as
# U+00A0 (no-break space).
_SPACE = (
    " \t\f\v\ufeff\u00a0\u1680\u2000\u2001\u2002\u2003\u2004\u2005"
    "\u2006\u2007\u2008\u2009\u200a\u202f\u205f\u3000"
)
_SPACE_OR_LINE = _SPACE + "\n\r"
_NATIVE = re.compile(r"%([A-Za-z_$][A-Za-z0-9_$]*)\(")
_WORD_CHARS = frozenset(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$"
)


def _word_before(text: str, end: int) -> tuple[str, int]:
    """Return the word that ends at `end`, and its start."""
    start = end
    while start > 0 and text[start - 1] in _WORD_CHARS:
        start -= 1
    return text[start:end], start


def _starts_call(
    text: str,
    pos: int,
    control_closes: set[int] | frozenset[int] = frozenset(),
) -> bool:
    """Return True if a runtime call can start at `pos` in `text`.

    `control_closes` holds the positions of the `)` that close the `(` of an
    `if`, `while`, or `for` head.
    """
    end = pos
    while end > 0 and text[end - 1] in _SPACE:
        end -= 1
    if end == 0 or text[end - 1] in "\n\r":
        return True
    if text[end - 1] in _PREFIX_CHARS:
        return True
    if text[end - 1] == ")":
        return end - 1 in control_closes
    word, start = _word_before(text, end)
    return word in _PREFIX_WORDS and not (start and text[start - 1] == ".")


def _opens_control(text: str, pos: int) -> bool:
    """Return True if the `(` at `pos` follows `if`, `while`, or `for`."""
    end = pos
    while end > 0 and text[end - 1] in _SPACE_OR_LINE:
        end -= 1
    word, start = _word_before(text, end)
    if word == "await":
        # for await (...)
        end = start
        while end > 0 and text[end - 1] in _SPACE_OR_LINE:
            end -= 1
        word, start = _word_before(text, end)
        if word != "for":
            return False
    return word in _CONTROL_WORDS and not (start and text[start - 1] == ".")


def rewrite_natives(text: str) -> str:
    """Change V8 runtime calls into valid JavaScript.

    `%Name(` becomes `__v8native("Name")(`. The scan skips string literals,
    template literals, and comments. It does not track regular expressions.
    It pairs the parentheses of the code, to find the end of an `if`,
    `while`, or `for` head.
    """
    out: list[str] = []
    length = len(text)
    opened: list[bool] = []  # for each open `(`: does it start a head?
    control_closes: set[int] = set()
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
            if match and _starts_call(text, i, control_closes):
                out.append('__v8native("%s")(' % match.group(1))
                opened.append(False)
                i = match.end()
                continue
            j = i + 1
        elif ch == "(":
            opened.append(_opens_control(text, i))
            j = i + 1
        elif ch == ")":
            if opened and opened.pop():
                control_closes.add(i)
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
# At the pins above, grammar_parser-javascript takes more than 60 s for each
# file, alone and with one thread. All of them, except
# v8-earley-boyer-strict.js, have array elisions (`[,,,]`).
EXCLUDE: frozenset[str] = frozenset(
    {
        "mjsunit/es6/spread-array-misc.js",
        "mjsunit/regress/regress-818438.js",
        "mjsunit/regress/regress-crbug-1027025.js",
        "jstests/stress/array-flatten.js",
        "jstests/stress/v8-earley-boyer-strict.js",
    }
)

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
    missing = []
    for path in paths:
        listing = _git(
            work, url, "ls-tree", "-d", "FETCH_HEAD", "--", path, capture=True
        )
        if listing.returncode != 0:
            raise SystemExit(
                "[-] git ls-tree failed for %s" % url
            )
        if not listing.stdout.strip():
            missing.append(path)
    if missing:
        raise SystemExit(
            "[-] %s at %s: missing path %s" % (url, commit, ", ".join(missing))
        )
    _git_ok(work, url, "checkout", "-q", "--force", "FETCH_HEAD")
    head = _head(work, url)
    if not head:
        raise SystemExit("[-] %s: cannot read HEAD" % url)
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


# The time limit of one engine run in seconds (AFL++ EXEC_TIMEOUT).
DEFAULT_TIMEOUT_S = 1.0


def suite_of(name: str) -> str:
    """Return the suite of a seed name: the text before the first `_`."""
    return name.split("_", 1)[0]


@dataclass
class FilterSummary:
    seeds: Counter
    kept: Counter
    failed: Counter
    timed_out: Counter
    crashed: dict
    signals: dict = field(default_factory=dict)


class CliError(SystemExit):
    """A user error. `main` prints it as one line and returns 2."""

    def __init__(self, message: str) -> None:
        super().__init__(2)
        self.message = message


def _check_folders(seeds_dir: Path, out_dir: Path) -> None:
    if not seeds_dir.is_dir():
        raise CliError("[-] %s is not a folder" % seeds_dir)
    if out_dir.exists() and (
        not out_dir.is_dir() or any(out_dir.iterdir())
    ):
        raise CliError("[-] %s is not an empty folder" % out_dir)


def _run_one(argv: list[str], timeout: float) -> str:
    """Run the engine once. Return ok, failed, timeout, or crash:<name>."""
    # A new session gives the run its own process group. A time-out then
    # also stops the children of the engine command.
    try:
        proc = subprocess.Popen(
            argv,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
    except OSError as err:
        raise CliError("[-] Cannot run %s: %s" % (argv[0], err))
    try:
        proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except OSError:
            pass
        proc.communicate()
        return "timeout"
    # Stop the children that are still alive.
    try:
        os.killpg(proc.pid, signal.SIGKILL)
    except OSError:
        pass
    if proc.returncode == 0:
        return "ok"
    if proc.returncode < 0:
        try:
            name = signal.Signals(-proc.returncode).name
        except ValueError:
            name = "signal %d" % -proc.returncode
        return "crash:" + name
    return "failed"


def run_filter(
    template: list[str],
    shims: Path,
    seeds_dir: Path,
    out_dir: Path,
    timeout: float,
    jobs: int,
) -> FilterSummary:
    """Copy the seeds that the engine runs in time to `out_dir`.

    `template` is the engine command. `{shims}` and `{seed}` in its
    arguments become the shims path and the seed path.
    """
    _check_folders(seeds_dir, out_dir)
    files = sorted(p for p in seeds_dir.rglob("*") if p.is_file())

    stop: list[bool] = []

    def work(path: Path) -> str:
        if stop:
            return "failed"
        values = {"shims": str(shims), "seed": str(path)}
        argv = [
            re.sub(r"\{(shims|seed)\}", lambda m: values[m.group(1)], arg)
            for arg in template
        ]
        try:
            return _run_one(argv, timeout)
        except CliError:
            stop.append(True)
            raise

    summary = FilterSummary(
        Counter(), Counter(), Counter(), Counter(), {}
    )
    with ThreadPoolExecutor(max(1, jobs)) as pool:
        results = list(pool.map(work, files))
    for path, result in zip(files, results):
        suite = suite_of(path.name)
        summary.seeds[suite] += 1
        if result == "ok":
            summary.kept[suite] += 1
            target = out_dir / path.relative_to(seeds_dir)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(str(path), str(target))
        elif result == "timeout":
            summary.timed_out[suite] += 1
        elif result.startswith("crash:"):
            summary.crashed.setdefault(suite, []).append(path.name)
            summary.signals[path.name] = result[6:]
        else:
            summary.failed[suite] += 1
    return summary


def count_by_suite(folder: Path) -> Counter:
    """Count the regular files under `folder` for each suite."""
    if not folder.is_dir():
        raise CliError("[-] %s is not a folder" % folder)
    counts: Counter = Counter()
    for path in folder.rglob("*"):
        if path.is_file():
            counts[suite_of(path.name)] += 1
    return counts


def _table(header: list[str], rows: list[list[str]]) -> str:
    widths = [
        max(len(row[i]) for row in [header] + rows)
        for i in range(len(header))
    ]
    lines = []
    for row in [header] + rows:
        cells = [
            cell.ljust(widths[0]) if i == 0 else cell.rjust(widths[i])
            for i, cell in enumerate(row)
        ]
        lines.append("  ".join(cells).rstrip())
    return "\n".join(lines)


def _filter(args: argparse.Namespace) -> int:
    _check_folders(args.seeds, args.out)
    template = shlex.split(args.cmd)
    if not template:
        print("[-] Empty --cmd", file=sys.stderr)
        return 2
    engine = template[0]
    if shutil.which(engine) is None:
        print("[-] Cannot run %s" % engine, file=sys.stderr)
        return 2
    shims = args.shims or args.seeds.parent / "shims.js"
    if not shims.is_file():
        print("[-] Cannot find the shims file %s" % shims, file=sys.stderr)
        return 2
    summary = run_filter(
        template,
        shims,
        args.seeds,
        args.out,
        args.timeout,
        args.jobs,
    )
    names = sorted(summary.seeds)
    rows = []
    for name in names:
        rows.append(
            [name]
            + [
                str(counter[name])
                for counter in (
                    summary.seeds,
                    summary.kept,
                    summary.failed,
                    summary.timed_out,
                )
            ]
            + [str(len(summary.crashed.get(name, [])))]
        )
    total = ["total"] + [
        str(sum(int(row[i]) for row in rows)) for i in range(1, 6)
    ]
    print(
        _table(
            ["Suite", "Seeds", "Kept", "Failed", "Timeout", "Crash"],
            rows + [total],
        )
    )
    for name in sorted(summary.crashed):
        for file in sorted(summary.crashed[name]):
            print("crash %s: %s (%s)" % (name, file, summary.signals[file]))
    return 0


def _count(args: argparse.Namespace) -> int:
    seeds = count_by_suite(args.seeds)
    trees: Counter = Counter()
    if args.trees is not None and not args.trees.is_dir():
        raise CliError("[-] %s is not a folder" % args.trees)
    header = ["Suite", "Seeds"]
    if args.trees is not None:
        trees = Counter(
            suite_of(p.name) for p in args.trees.iterdir() if p.is_file()
        )
        header.append("Trees")
    rows = []
    for name in sorted(set(seeds) | set(trees)):
        row = [name, str(seeds[name])]
        if args.trees is not None:
            row.append(str(trees[name]))
        rows.append(row)
    total = ["total", str(sum(seeds.values()))]
    if args.trees is not None:
        total.append(str(sum(trees.values())))
    print(_table(header, rows + [total]))
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Turn engine test suites into JavaScript seeds."
    )
    sub = parser.add_subparsers(dest="command", required=True)
    fetch = sub.add_parser("fetch", help="download the suites, select seeds")
    fetch.add_argument("dest", type=Path, help="working folder")
    filt = sub.add_parser("filter", help="keep seeds that an engine runs")
    filt.add_argument(
        "--cmd",
        required=True,
        help="engine command; {shims} and {seed} are placeholders",
    )
    filt.add_argument("--shims", type=Path, help="default: SEEDS/../shims.js")
    filt.add_argument(
        "--timeout", type=float, default=DEFAULT_TIMEOUT_S, metavar="S"
    )
    filt.add_argument(
        "-j", "--jobs", type=int, default=os.cpu_count() or 1, metavar="N"
    )
    filt.add_argument("seeds", type=Path, help="folder of seeds")
    filt.add_argument("out", type=Path, help="empty folder for kept seeds")
    count = sub.add_parser("count", help="count seeds for each suite")
    count.add_argument("seeds", type=Path, help="folder of seeds")
    count.add_argument("trees", type=Path, nargs="?", help="folder of trees")
    args = parser.parse_args(argv)
    if args.command == "filter" and not args.timeout > 0:
        parser.error("--timeout must be greater than 0")
    if args.command == "filter" and args.jobs < 1:
        parser.error("-j must be at least 1")
    try:
        if args.command == "filter":
            return _filter(args)
        if args.command == "count":
            return _count(args)
        return _fetch(args.dest)
    except CliError as err:
        print(err.message, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
