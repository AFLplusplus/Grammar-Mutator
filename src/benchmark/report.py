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
"""Write the generated part of the README.md of a benchmark result.

Usage: report.py RESULT_DIR [--check]
       report.py -h | --help

RESULT_DIR holds the raw output files (*.txt) of benchmark-<name> and
platform.sh. The script writes the tables between two marker lines of
RESULT_DIR/README.md. It does not change the text outside the markers.
With --check, it writes nothing. It exits with 1 if the file differs.
"""

from __future__ import annotations

import math
import statistics
import sys
from dataclasses import dataclass, field
from pathlib import Path

START_MARKER = "<!-- report.py: start of the generated part -->"
END_MARKER = "<!-- report.py: end of the generated part -->"

STAGES = ("rules", "random", "recursive", "splice")
STAGE_TITLES = {
    "rules": "Rules",
    "random": "Random",
    "recursive": "Recursive",
    "splice": "Splice",
}
MISSING = "–"

NOTE = (
    "The duplicate rates go up with the number of steps. "
    "Compare results only at the same options and on the same platform. "
    "– means that the raw file has no record for the cell. "
    "An entry without a recursion edge has no recursive stage."
)

# The generated part. Each field is a finished table or text block. A table
# holds the rows of all grammars, so that the layout does not change with the
# number of grammars.
REPORT = """\
## Platform

```
{platform}
```

## Runs

{runs}

{warnings}## Parse time

The tables show the time of the first load of each seed in µs. This time
includes the parse, the tree file write, and the chunk store add. All seeds
count, including the seeds without a tree. The time of the first seed in name
order also includes the one-time setup of the parser.

{parse}

## Time per output

Time in µs for each output, for each stage.

{times}

## Duplicates

Duplicate outputs and outputs equal to the seed, in percent.

{rates}

## Memory

{memory}

## Digests

{digests}

{note}
"""

# A new README.md: the parts to write by hand, then the generated part.
SKELETON = """\
# Benchmark result of {title}

## What it measured

{placeholder}

## Build

{placeholder}

## Seeds

{placeholder}

## Commands

{placeholder}

{block}
"""

# The size bands of the parse-time table: (lower limit, upper limit, name).
# A seed is in a band if lower limit <= bytes < upper limit.
SIZE_BANDS = (
    (0, 1024, "under 1 KB"),
    (1024, 10 * 1024, "1–10 KB"),
    (10 * 1024, 100 * 1024, "10–100 KB"),
    (100 * 1024, math.inf, "100 KB or more"),
)

NO_RUN = "No run in this result."
NO_SEED = "No seed record in this result."
NO_DIGEST = "No digest in this result."

USAGE = "usage: report.py RESULT_DIR [--check]\n       report.py -h | --help"

PLACEHOLDER = "Write this section by hand."


class ReportError(Exception):
    """An error in the input. The message names the file."""


@dataclass
class Record:
    """One line of a raw file: a kind and its key=value fields."""

    kind: str
    fields: dict
    where: str

    def text(self, key: str) -> str:
        if key not in self.fields:
            raise ReportError(f"{self.where}: no '{key}' in the {self.kind} record")
        return self.fields[key]

    def integer(self, key: str) -> int:
        value = self.text(key)
        try:
            return int(value)
        except ValueError:
            raise ReportError(
                f"{self.where}: '{key}={value}' is not a whole number"
            ) from None

    def number(self, key: str) -> float:
        value = self.text(key)
        try:
            return float(value)
        except ValueError:
            raise ReportError(
                f"{self.where}: '{key}={value}' is not a number"
            ) from None


@dataclass
class Entry:
    name: str
    bytes: int
    nodes: int
    stages: dict = field(default_factory=dict)  # stage name -> Record
    trim_us: float | None = None


@dataclass
class Seed:
    name: str
    bytes: int
    parse_us: float
    tree: bool


@dataclass
class Run:
    name: str
    grammar: str
    options: str
    seeds: int
    trees: int
    store_chunks: int
    store_seeds: int
    fill_ms: float
    entries: list
    peak_kb: int
    wall_ms: float | None
    seeds_parsed: list = field(default_factory=list)  # Seed for each seed record


@dataclass
class Digest:
    name: str
    grammar: str
    options: str
    seeds: int
    trees: int
    outputs: int
    new_entries: int
    trims: int
    hash: str
    wall_ms: float | None


@dataclass
class Result:
    platform: str = ""
    runs: list = field(default_factory=list)
    digests: list = field(default_factory=list)
    unoptimized: list = field(default_factory=list)  # names of the raw files


def parse_records(name: str, content: str) -> list:
    """Split the text of a raw file into records. Raise ReportError for a bad line."""
    records = []
    for number, line in enumerate(content.splitlines(), 1):
        if not line.strip():
            continue
        where = f"{name}:{number}"
        words = line.split()
        fields = {}
        for word in words[1:]:
            key, sep, value = word.partition("=")
            if not sep or not key:
                raise ReportError(f"{where}: '{word}' is not key=value")
            fields[key] = value
        records.append(Record(words[0], fields, where))
    return records


def once(current: object, rec: Record) -> None:
    """Raise ReportError if a record that may come once has come before."""
    if current is not None:
        raise ReportError(f"{rec.where}: the {rec.kind} record is there more than once")


def build_run(path: Path, records: list) -> Run:
    head = records[0]
    entries = []
    by_name = {}
    seeds = []
    store = None
    peak = None
    wall = None

    for rec in records[1:]:
        if rec.kind == "seed":
            seeds.append(
                Seed(
                    rec.text("name"),
                    rec.integer("bytes"),
                    rec.number("parse_us"),
                    rec.integer("tree") != 0,
                )
            )
        elif rec.kind == "store":
            once(store, rec)
            store = rec
        elif rec.kind == "entry":
            entry = Entry(rec.text("name"), rec.integer("bytes"), rec.integer("nodes"))
            if entry.name in by_name:
                raise ReportError(
                    f"{rec.where}: entry '{entry.name}' is there more than once"
                )
            entries.append(entry)
            by_name[entry.name] = entry
        elif rec.kind in ("stage", "trim"):
            name = rec.text("entry")
            if name not in by_name:
                raise ReportError(f"{rec.where}: no entry record for '{name}'")
            entry = by_name[name]
            if rec.kind == "trim":
                if entry.trim_us is not None:
                    raise ReportError(
                        f"{rec.where}: the trim record of entry '{name}'"
                        " is there more than once"
                    )
                entry.trim_us = rec.number("us_per_step")
                continue
            stage = rec.text("stage")
            if stage not in STAGES:
                raise ReportError(f"{rec.where}: unknown stage '{stage}'")
            if stage in entry.stages:
                raise ReportError(
                    f"{rec.where}: the {stage} stage of entry '{name}'"
                    " is there more than once"
                )
            rec.integer("steps")
            for key in ("us_per_output", "dup_pct", "same_pct"):
                rec.number(key)
            entry.stages[stage] = rec
        elif rec.kind == "peak":
            once(peak, rec)
            peak = rec.integer("rss_kb")
        elif rec.kind == "elapsed":
            once(wall, rec)
            wall = rec.number("wall_ms")
        else:
            raise ReportError(f"{rec.where}: unknown record '{rec.kind}'")
    if store is None:
        raise ReportError(f"{path.name}: no store record")
    if peak is None:
        raise ReportError(f"{path.name}: no peak record")
    options = []
    seed = head.integer("seed")
    if seed:
        options.append(f"-s {seed}")
    options.append(f"-e {head.integer('entries')}")
    options.append(f"-n {head.integer('steps')}")
    options.append(f"-t {head.integer('trims')}")
    return Run(
        path.name,
        head.text("grammar"),
        " ".join(options),
        head.integer("seeds"),
        head.integer("trees"),
        store.integer("chunks"),
        store.integer("seeds"),
        store.number("fill_ms"),
        entries,
        peak,
        wall,
        seeds,
    )


def build_digest(path: Path, records: list) -> Digest:
    head = records[0]
    digest = None
    wall = None

    for rec in records[1:]:
        if rec.kind == "digest":
            once(digest, rec)
            digest = rec
        elif rec.kind == "elapsed":
            once(wall, rec)
            wall = rec.number("wall_ms")
        else:
            raise ReportError(f"{rec.where}: unknown record '{rec.kind}'")
    if digest is None:
        raise ReportError(f"{path.name}: no digest record")
    options = []
    seed = head.integer("seed")
    if seed:
        options.append(f"-s {seed}")
    options.append(f"-n {head.integer('steps')}")
    options.append(f"-t {head.integer('trims')}")
    return Digest(
        path.name,
        head.text("grammar"),
        " ".join(options),
        head.integer("seeds"),
        head.integer("trees"),
        digest.integer("outputs"),
        digest.integer("new_entries"),
        digest.integer("trims"),
        digest.text("hash"),
        wall,
    )


def load_result(folder: Path) -> Result:
    """Read every *.txt file of a result folder."""
    files = sorted(Path(folder).glob("*.txt"))
    if not files:
        raise ReportError(f"{folder}: no .txt file")
    result = Result()
    platform_name = None
    seen = {}
    shared = {}  # field -> (value, file name)
    for path in files:
        try:
            content = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as exc:
            raise ReportError(f"{path.name}: cannot read the file: {exc}") from None
        first = next((ln for ln in content.splitlines() if ln.strip()), "")
        if first.startswith("date:"):
            if platform_name is not None:
                raise ReportError(
                    f"{path.name}: second platform file (the first is {platform_name})"
                )
            platform_name = path.name
            result.platform = "\n".join(content.splitlines())
            continue
        if not first.startswith("benchmark "):
            raise ReportError(f"{path.name}: not a benchmark output or a platform file")
        records = parse_records(path.name, content)
        head = records[0]
        mode = head.text("mode")
        grammar = head.text("grammar")
        if mode not in ("run", "digest"):
            raise ReportError(f"{path.name}: the mode '{mode}' is not run or digest")
        for name in ("version", "system", "machine"):
            value = head.text(name)
            if name in shared and shared[name][0] != value:
                raise ReportError(
                    f"{path.name}: {name}={value} differs from"
                    f" {name}={shared[name][0]} in {shared[name][1]}"
                )
            shared.setdefault(name, (value, path.name))
        if head.fields.get("optimized") == "0":
            result.unoptimized.append(path.name)
        key = (grammar, mode)
        if key in seen:
            raise ReportError(
                f"{path.name}: the same grammar '{grammar}' and mode '{mode}'"
                f" as {seen[key]}"
            )
        seen[key] = path.name
        if mode == "run":
            result.runs.append(build_run(path, records))
        else:
            result.digests.append(build_digest(path, records))
    if platform_name is None:
        raise ReportError(
            f"{folder}: no platform file (a file that starts with 'date:')"
        )
    if not result.runs and not result.digests:
        raise ReportError(f"{folder}: no run file and no digest file")
    return result


def us(value: float) -> str:
    """Microseconds: one decimal below 10, else a whole number."""
    if round(value, 1) < 10:
        return f"{value:.1f}"
    return f"{round(value):,}"


def count(value: int) -> str:
    return f"{value:,}"


def seconds(ms: float | None) -> str:
    """A time in ms: below 1 s in ms, else in seconds. Both have one decimal."""
    if ms is None:
        return MISSING
    if round(ms, 1) < 1000:
        return f"{ms:.1f} ms"
    return f"{ms / 1000.0:.1f} s"


def table(header: list, rows: list, empty: str = "") -> str:
    """A Markdown table. Without rows, the text EMPTY."""
    if not rows:
        return empty
    lines = ["| " + " | ".join(header) + " |", "|" + "---|" * len(header)]
    lines += ["| " + " | ".join(row) + " |" for row in rows]
    return "\n".join(lines)


def run_rows(result: Result) -> list:
    files = [(r.grammar, 0, "run", r) for r in result.runs]
    files += [(d.grammar, 1, "digest", d) for d in result.digests]
    files.sort(key=lambda f: (f[0], f[1]))
    return [
        [
            grammar,
            mode,
            f"`{item.options}`",
            count(item.seeds),
            count(item.trees),
            seconds(item.wall_ms),
        ]
        for grammar, _, mode, item in files
    ]


def time_rows(runs: list) -> list:
    rows = []
    for run in runs:
        for e in run.entries:
            row = [run.grammar, e.name, count(e.bytes), count(e.nodes)]
            for s in STAGES:
                rec = e.stages.get(s)
                row.append(MISSING if rec is None else us(rec.number("us_per_output")))
            row.append(MISSING if e.trim_us is None else us(e.trim_us))
            rows.append(row)
    return rows


def rate_rows(runs: list) -> list:
    rows = []
    for run in runs:
        for e in run.entries:
            row = [run.grammar, e.name]
            for s in STAGES:
                rec = e.stages.get(s)
                if rec is None:
                    row += [MISSING, MISSING]
                else:
                    row += [rec.text("dup_pct"), rec.text("same_pct")]
            rows.append(row)
    return rows


def nearest_rank(values: list, fraction: float) -> float:
    """The percentile by the nearest-rank method: always one of the values."""
    ordered = sorted(values)
    rank = max(1, math.ceil(fraction * len(ordered)))
    return ordered[rank - 1]


def parse_rows(runs: list) -> list:
    rows = []
    for run in runs:
        seeds = run.seeds_parsed
        if not seeds:
            continue
        times = [s.parse_us for s in seeds]
        slowest = max(seeds, key=lambda s: s.parse_us)
        # A seed of 0 bytes has no time per KB, so it is not in this median
        per_kb = [s.parse_us / (s.bytes / 1024) for s in seeds if s.bytes > 0]
        rows.append(
            [
                run.grammar,
                count(len(seeds)),
                count(sum(1 for s in seeds if not s.tree)),
                us(statistics.median(times)),
                us(nearest_rank(times, 0.9)),
                us(slowest.parse_us),
                slowest.name,
                us(statistics.median(per_kb)) if per_kb else MISSING,
            ]
        )
    return rows


def size_rows(runs: list) -> list:
    rows = []
    for run in runs:
        for low, high, name in SIZE_BANDS:
            times = [s.parse_us for s in run.seeds_parsed if low <= s.bytes < high]
            if times:
                rows.append(
                    [run.grammar, name, count(len(times)), us(statistics.median(times))]
                )
    return rows


def render_parse(runs: list) -> str:
    """The parse-time tables, or one line if there is no seed record."""
    if not runs:
        return NO_RUN
    summary = parse_rows(runs)
    if not summary:
        return NO_SEED
    header = ["Grammar", "Seeds", "No tree", "Median", "90th percentile", "Slowest"]
    header += ["Slowest seed", "Median µs per KB"]
    sizes = table(["Grammar", "Size", "Seeds", "Median"], size_rows(runs))
    return table(header, summary) + "\n\n" + sizes


def memory_rows(runs: list) -> list:
    return [
        [
            run.grammar,
            count(run.store_chunks),
            count(run.store_seeds),
            seconds(run.fill_ms),
            f"{count(run.peak_kb)} KB",
        ]
        for run in runs
    ]


def digest_rows(digests: list) -> list:
    return [
        [
            d.grammar,
            f"`{d.options}`",
            count(d.outputs),
            count(d.new_entries),
            count(d.trims),
            f"`{d.hash}`",
        ]
        for d in digests
    ]


def render(result: Result) -> str:
    """Return the generated part, without the marker lines."""
    runs = sorted(result.runs, key=lambda r: r.grammar)
    digests = sorted(result.digests, key=lambda d: d.grammar)
    stage_titles = [STAGE_TITLES[s] for s in STAGES]
    rate_titles = []
    for title in stage_titles:
        rate_titles += [f"{title} dup %", f"{title} same %"]
    warnings = "".join(
        f"Warning: {name} comes from a build without optimization.\n\n"
        for name in sorted(result.unoptimized)
    )
    return REPORT.format(
        platform=result.platform,
        runs=table(
            ["Grammar", "Mode", "Options", "Seeds", "Trees", "Wall time"],
            run_rows(result),
        ),
        warnings=warnings,
        parse=render_parse(runs),
        times=table(
            ["Grammar", "Entry", "Bytes", "Nodes"] + stage_titles + ["Trim (µs/step)"],
            time_rows(runs),
            NO_RUN,
        ),
        rates=table(["Grammar", "Entry"] + rate_titles, rate_rows(runs), NO_RUN),
        memory=table(
            ["Grammar", "Chunks", "Trees", "Fill time", "Peak memory"],
            memory_rows(runs),
            NO_RUN,
        ),
        digests=table(
            ["Grammar", "Options", "Outputs", "New entries", "Trim steps", "Hash"],
            digest_rows(digests),
            NO_DIGEST,
        ),
        note=NOTE,
    )


def update_readme(
    text: str | None, generated: str, title: str, where: str = "README.md"
) -> str:
    """Put the generated part between the markers of the README text.

    TITLE is the name of the result folder. It names a new README.
    WHERE is the path of the README. It starts the text of an error.
    """
    block = f"{START_MARKER}\n\n{generated}\n{END_MARKER}"
    if text is None:
        return SKELETON.format(title=title, placeholder=PLACEHOLDER, block=block)
    starts = text.count(START_MARKER)
    ends = text.count(END_MARKER)
    if starts != 1 or ends != 1:
        raise ReportError(
            f"{where}: need exactly one start marker and one end marker"
            f" (found {starts} and {ends})"
        )
    begin = text.index(START_MARKER)
    end = text.index(END_MARKER)
    if end < begin:
        raise ReportError(f"{where}: the end marker is before the start marker")
    return text[:begin] + block + text[end + len(END_MARKER) :]


def read_readme(folder: Path) -> str | None:
    path = Path(folder) / "README.md"
    if not path.exists():
        return None
    # newline="" keeps the line endings as they are
    with open(path, encoding="utf-8", newline="") as handle:
        return handle.read()


def expected_readme(folder: Path) -> str:
    folder = Path(folder)
    generated = render(load_result(folder))
    return update_readme(
        read_readme(folder), generated, folder.resolve().name, str(folder / "README.md")
    )


def check(folder: Path) -> bool:
    """Return True if README.md is what the script writes."""
    current = read_readme(folder)
    return current is not None and current == expected_readme(folder)


def main(argv: list) -> int:
    if any(a in ("-h", "--help") for a in argv):
        print(USAGE)
        return 0
    args = [a for a in argv if a != "--check"]
    checking = len(args) != len(argv)
    if (
        len(argv) - len(args) > 1
        or len(args) != 1
        or args[0].startswith("-")
    ):
        print(USAGE, file=sys.stderr)
        return 2
    folder = Path(args[0])
    try:
        if not folder.is_dir():
            raise ReportError(f"{folder}: not a folder")
        wanted = expected_readme(folder)
        if checking:
            if read_readme(folder) != wanted:
                print(
                    f"report.py: {folder / 'README.md'} is not up to date",
                    file=sys.stderr,
                )
                return 1
            return 0
        with open(folder / "README.md", "w", encoding="utf-8", newline="") as handle:
            handle.write(wanted)
    except ReportError as exc:
        print(f"report.py: {exc}", file=sys.stderr)
        return 1
    except OSError as exc:
        print(
            f"report.py: {exc.filename or folder}: {exc.strerror or exc}",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
