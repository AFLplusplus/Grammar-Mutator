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

RESULT_DIR holds the raw output files (*.txt) of benchmark-<name> and
platform.sh. The script writes the tables between two marker lines of
RESULT_DIR/README.md. It does not change the text outside the markers.
With --check, it writes nothing. It exits with 1 if the file differs.
"""

from __future__ import annotations

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
    "Compare results only at the same options and on the same platform.\n"
)

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
    platform: str | None = None
    runs: list = field(default_factory=list)
    digests: list = field(default_factory=list)


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
    store = None
    peak = None
    wall = None

    for rec in records[1:]:
        if rec.kind == "seed":
            rec.text("name")
            rec.integer("bytes")
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
                entry.trim_us = rec.number("us_per_step")
                continue
            stage = rec.text("stage")
            if stage not in STAGES:
                raise ReportError(f"{rec.where}: unknown stage '{stage}'")
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


def table(header: list, rows: list) -> list:
    lines = ["| " + " | ".join(header) + " |"]
    lines.append("|" + "---|" * len(header))
    for row in rows:
        lines.append("| " + " | ".join(row) + " |")
    return lines


def render_run(run: Run) -> list:
    out = [f"## {run.grammar}", ""]
    header = ["Entry", "Bytes", "Nodes"]
    header += [STAGE_TITLES[s] for s in STAGES] + ["Trim (µs/step)"]
    rows = []
    for e in run.entries:
        row = [e.name, count(e.bytes), count(e.nodes)]
        for s in STAGES:
            rec = e.stages.get(s)
            row.append(MISSING if rec is None else us(rec.number("us_per_output")))
        row.append(MISSING if e.trim_us is None else us(e.trim_us))
        rows.append(row)
    out += ["Time in µs for each output, for each stage.", ""]
    out += table(header, rows) + [""]

    header = ["Entry"]
    for s in STAGES:
        header += [f"{STAGE_TITLES[s]} dup %", f"{STAGE_TITLES[s]} same %"]
    rows = []
    for e in run.entries:
        row = [e.name]
        for s in STAGES:
            rec = e.stages.get(s)
            if rec is None:
                row += [MISSING, MISSING]
            else:
                row += [rec.text("dup_pct"), rec.text("same_pct")]
        rows.append(row)
    out += ["Duplicate outputs and outputs equal to the seed, in percent.", ""]
    out += table(header, rows) + [""]

    out.append(
        f"- Chunk store: {count(run.store_chunks)} chunks from "
        f"{count(run.store_seeds)} trees. The fill took {seconds(run.fill_ms)}."
    )
    out.append(f"- Peak memory: {count(run.peak_kb)} KB.")
    out.append("")
    return out


def render(result: Result) -> str:
    """Return the generated part, without the marker lines."""
    out = []
    if result.platform is not None:
        out += ["## Platform", "", "```", result.platform, "```", ""]

    files = [(r.grammar, 0, r.name, "run", r) for r in result.runs]
    files += [(d.grammar, 1, d.name, "digest", d) for d in result.digests]
    files.sort(key=lambda f: (f[0], f[1]))
    rows = []
    for grammar, _, name, mode, item in files:
        rows.append(
            [
                grammar,
                mode,
                f"`{item.options}`",
                count(item.seeds),
                count(item.trees),
                seconds(item.wall_ms),
            ]
        )
    out += ["## Runs", ""]
    out += table(["Grammar", "Mode", "Options", "Seeds", "Trees", "Wall time"], rows)
    out.append("")

    for run in sorted(result.runs, key=lambda r: r.grammar):
        out += render_run(run)

    if result.digests:
        rows = []
        for d in sorted(result.digests, key=lambda d: d.grammar):
            rows.append(
                [
                    d.grammar,
                    f"`{d.options}`",
                    count(d.outputs),
                    count(d.new_entries),
                    count(d.trims),
                    f"`{d.hash}`",
                ]
            )
        out += ["## Digests", ""]
        out += table(
            ["Grammar", "Options", "Outputs", "New entries", "Trim steps", "Hash"],
            rows,
        )
        out.append("")

    out.append(NOTE.rstrip("\n"))
    return "\n".join(out) + "\n"


def update_readme(
    text: str | None, generated: str, title: str, where: str = "README.md"
) -> str:
    """Put the generated part between the markers of the README text.

    TITLE is the name of the result folder. It names a new README.
    WHERE is the path of the README. It starts the text of an error.
    """
    block = f"{START_MARKER}\n\n{generated}\n{END_MARKER}"
    if text is None:
        parts = [f"# Benchmark result of {title}\n"]
        for heading in ("What it measured", "Build", "Seeds", "Commands"):
            parts.append(f"## {heading}\n\n{PLACEHOLDER}\n")
        parts.append(block + "\n")
        return "\n".join(parts)
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
    args = [a for a in argv if a != "--check"]
    checking = len(args) != len(argv)
    if len(argv) - len(args) > 1 or len(args) != 1:
        print("usage: report.py RESULT_DIR [--check]", file=sys.stderr)
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
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
