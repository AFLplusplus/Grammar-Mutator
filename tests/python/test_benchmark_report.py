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
"""Unit tests for src/benchmark/report.py.

Run them with `make test-python`.
"""

from __future__ import annotations

import contextlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "src" / "benchmark"))

import report

PLATFORM = "date: 2026-11-27\nsystem: TestOS 1\ncpu: Test CPU\n"

RUBY_RUN = """\
benchmark version=1.2.0 grammar=ruby system=Darwin machine=arm64 optimized=1 mode=run seed=0 entries=2 steps=5000 trims=2000 parse_budget_ms=0 seeds=300 trees=300
seed name=0 bytes=131 parse_us=1728.0 tree=1
store seeds=300 chunks=2302 fill_ms=1234.5
entry name=0 bytes=131 nodes=3 rules_pairs=9 repick_us=1.0 count_us=1.0
stage entry=0 stage=rules steps=9 us_per_output=9.0 avg_bytes=140 dup_pct=0.0 same_pct=0.0
stage entry=0 stage=random steps=5000 us_per_output=1.0 avg_bytes=120 dup_pct=66.5 same_pct=25.0
stage entry=0 stage=splice steps=5000 us_per_output=2.1 avg_bytes=150 dup_pct=46.2 same_pct=22.9
trim entry=0 steps=10 us_per_step=0.5
entry name=244 bytes=344 nodes=175 rules_pairs=500 repick_us=1.0 count_us=1.0
stage entry=244 stage=rules steps=500 us_per_output=4641.2 avg_bytes=340 dup_pct=0.7 same_pct=0.7
stage entry=244 stage=random steps=5000 us_per_output=5.7 avg_bytes=330 dup_pct=70.0 same_pct=30.0
stage entry=244 stage=recursive steps=5000 us_per_output=15.1 avg_bytes=580 dup_pct=98.1 same_pct=0.0
stage entry=244 stage=splice steps=5000 us_per_output=6.5 avg_bytes=339 dup_pct=46.2 same_pct=22.9
trim entry=244 steps=102 us_per_step=5.7
peak rss_kb=9600
"""

JS_RUN = """\
benchmark version=1.2.0 grammar=javascript system=Darwin machine=arm64 optimized=1 mode=run seed=0 entries=1 steps=5000 trims=2000 parse_budget_ms=0 seeds=1035 trees=1013
seed name=a.js bytes=755 parse_us=2376.0 tree=1
store seeds=1013 chunks=365859 fill_ms=22931.1
entry name=a.js bytes=635 nodes=19 rules_pairs=79 repick_us=21.0 count_us=1.0
stage entry=a.js stage=rules steps=79 us_per_output=31.2 avg_bytes=819 dup_pct=1.3 same_pct=0.0
stage entry=a.js stage=random steps=5000 us_per_output=7.8 avg_bytes=694 dup_pct=40.1 same_pct=12.8
stage entry=a.js stage=recursive steps=5000 us_per_output=22.7 avg_bytes=981 dup_pct=97.0 same_pct=0.1
stage entry=a.js stage=splice steps=5000 us_per_output=69.3 avg_bytes=1171 dup_pct=19.6 same_pct=6.7
trim entry=a.js steps=16 us_per_step=10.1
peak rss_kb=855632
elapsed wall_ms=132400.0
"""

RUBY_DIGEST = """\
benchmark version=1.2.0 grammar=ruby system=Darwin machine=arm64 optimized=1 mode=digest seed=0 steps=200 trims=20 parse_budget_ms=0 seeds=300 trees=300
digest outputs=1613412 new_entries=300 trims=13702 hash=fd87045834903cd1
"""

HAND_TEXT = (
    "# Title\n\n## Notes\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n"
    + report.START_MARKER
    + "\nold\n"
    + report.END_MARKER
    + "\n\n## Notes\n\n| c | d |\n|---|---|\n| 3 | 4 |"
)


def write(folder: Path, name: str, text: str) -> None:
    (folder / name).write_text(text, encoding="utf-8")


def run_main(argv: list) -> int:
    with contextlib.redirect_stderr(io.StringIO()):
        return report.main(argv)


class ReportTest(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.dir = Path(self._tmp.name) / "2026-11-27"
        self.dir.mkdir()
        write(self.dir, "platform.txt", PLATFORM)
        write(self.dir, "ruby-run.txt", RUBY_RUN)
        write(self.dir, "javascript-run.txt", JS_RUN)
        write(self.dir, "ruby-digest.txt", RUBY_DIGEST)

    def render(self) -> str:
        return report.render(report.load_result(self.dir))

    def test_time_table(self) -> None:
        out = self.render()
        self.assertIn("| ruby | 0 | 131 | 3 | 9.0 | 1.0 | – | 2.1 | 0.5 |", out)
        self.assertIn("| ruby | 244 | 344 | 175 | 4,641 | 5.7 | 15 | 6.5 | 5.7 |", out)
        self.assertIn("| javascript | a.js | 635 | 19 | 31 | 7.8 | 23 | 69 | 10 |", out)
        # one table for all grammars: the javascript row comes first
        self.assertLess(out.index("| javascript | a.js | 635"), out.index("| ruby | 0 | 131"))

    def test_missing_stage_and_elapsed(self) -> None:
        out = self.render()
        self.assertIn("| ruby | 0 | 0.0 | 0.0 | 66.5 | 25.0 | – | – | 46.2 | 22.9 |", out)
        self.assertIn("| ruby | run | `-e 2 -n 5000 -t 2000` | 300 | 300 | – |", out)

    def test_wall_time_seconds(self) -> None:
        self.assertIn(
            "| javascript | run | `-e 1 -n 5000 -t 2000` | 1,035 | 1,013 | 132.4 s |",
            self.render(),
        )

    def test_memory_table(self) -> None:
        out = self.render()
        self.assertIn(
            "| Grammar | Chunks | Trees | Fill time | Peak memory |", out
        )
        self.assertIn("| javascript | 365,859 | 1,013 | 22.9 s | 855,632 KB |", out)
        self.assertIn("| ruby | 2,302 | 300 | 1.2 s | 9,600 KB |", out)

    def test_digest_table(self) -> None:
        self.assertIn(
            "| ruby | `-n 200 -t 20` | 1,613,412 | 300 | 13,702 | `fd87045834903cd1` |",
            self.render(),
        )

    def test_platform_block(self) -> None:
        self.assertIn(
            "## Platform\n\n```\n" + PLATFORM + "```\n",
            self.render(),
        )

    def test_order(self) -> None:
        out = self.render()
        marks = [
            "## Platform",
            "## Runs",
            "## Time per output",
            "## Duplicates",
            "## Memory",
            "## Digests",
            "Compare results only",
        ]
        positions = [out.index(m) for m in marks]
        self.assertEqual(positions, sorted(positions))
        self.assertTrue(out.endswith("no recursive stage.\n"))
        # no section for each grammar
        self.assertNotIn("## ruby", out)
        self.assertNotIn("## javascript", out)

    def test_keeps_text_outside_markers(self) -> None:
        generated = self.render()
        new = report.update_readme(HAND_TEXT, generated, "x")
        before = HAND_TEXT[: HAND_TEXT.index(report.START_MARKER)]
        after = HAND_TEXT[HAND_TEXT.index(report.END_MARKER) + len(report.END_MARKER) :]
        self.assertTrue(new.startswith(before + report.START_MARKER))
        self.assertTrue(new.endswith(report.END_MARKER + after))
        self.assertIn("## Platform", new)
        self.assertNotIn("\nold\n", new)

    def test_second_run_no_change(self) -> None:
        generated = self.render()
        once = report.update_readme(HAND_TEXT, generated, "x")
        self.assertEqual(report.update_readme(once, generated, "x"), once)
        skeleton = report.update_readme(None, generated, "x")
        self.assertEqual(report.update_readme(skeleton, generated, "x"), skeleton)

    def test_skeleton(self) -> None:
        out = report.update_readme(None, self.render(), "2026-11-27")
        self.assertTrue(out.startswith("# Benchmark result of 2026-11-27\n"))
        for heading in ("What it measured", "Build", "Seeds", "Commands"):
            self.assertIn(f"\n## {heading}\n", out)
        self.assertIn(report.START_MARKER, out)
        self.assertIn(report.END_MARKER, out)
        self.assertLess(out.index("## Commands"), out.index(report.START_MARKER))

    def test_errors(self) -> None:
        empty = Path(self._tmp.name) / "empty"
        empty.mkdir()
        with self.assertRaises(report.ReportError):
            report.load_result(empty)
        self.assertEqual(run_main([str(empty)]), 1)

        write(self.dir, "other.txt", "hello\n")
        with self.assertRaises(report.ReportError) as ctx:
            report.load_result(self.dir)
        self.assertIn("other.txt", str(ctx.exception))
        self.assertIn("not a benchmark output", str(ctx.exception))
        (self.dir / "other.txt").unlink()

        bad = RUBY_RUN.replace(
            "stage entry=0 stage=rules steps=9", "stage entry=0 stage=rules steps=abc"
        )
        write(self.dir, "ruby-run.txt", bad)
        with self.assertRaises(report.ReportError) as ctx:
            report.load_result(self.dir)
        self.assertIn("ruby-run.txt:5", str(ctx.exception))
        self.assertIn("steps=abc", str(ctx.exception))
        self.assertEqual(run_main([str(self.dir)]), 1)
        write(self.dir, "ruby-run.txt", RUBY_RUN)

        write(self.dir, "ruby-run2.txt", RUBY_RUN)
        with self.assertRaises(report.ReportError) as ctx:
            report.load_result(self.dir)
        self.assertIn("ruby-run.txt", str(ctx.exception))
        self.assertIn("ruby-run2.txt", str(ctx.exception))
        (self.dir / "ruby-run2.txt").unlink()

        write(self.dir, "platform2.txt", PLATFORM)
        with self.assertRaises(report.ReportError):
            report.load_result(self.dir)
        (self.dir / "platform2.txt").unlink()

        write(self.dir, "README.md", report.START_MARKER + "\n")
        with self.assertRaises(report.ReportError) as ctx:
            report.check(self.dir)
        self.assertIn(str(self.dir / "README.md"), str(ctx.exception))
        self.assertIn("exactly one start marker", str(ctx.exception))
        self.assertEqual(run_main([str(self.dir)]), 1)
        both = "\n".join([report.START_MARKER] * 2 + [report.END_MARKER])
        with self.assertRaises(report.ReportError):
            report.update_readme(both, "x\n", "x")

    def test_short_times_in_ms(self) -> None:
        out = self.render()
        self.assertIn("| 22.9 s |", out)
        self.assertEqual(report.seconds(34.4), "34.4 ms")
        self.assertEqual(report.seconds(999.9), "999.9 ms")
        self.assertEqual(report.seconds(1000.0), "1.0 s")
        self.assertEqual(report.seconds(132400.0), "132.4 s")
        fast = RUBY_RUN.replace("fill_ms=1234.5", "fill_ms=34.4")
        write(self.dir, "ruby-run.txt", fast)
        self.assertIn("| ruby | 2,302 | 300 | 34.4 ms | 9,600 KB |", self.render())

    def test_repeated_records(self) -> None:
        cases = {
            "entry": "entry name=0 bytes=1 nodes=1 rules_pairs=1\n",
            "peak": "peak rss_kb=1\n",
            "store": "store seeds=1 chunks=1 fill_ms=1.0\n",
            "elapsed": "elapsed wall_ms=1.0\n",
        }
        for kind, line in cases.items():
            with self.subTest(kind=kind):
                text = JS_RUN + line if kind == "elapsed" else RUBY_RUN + line
                name = "javascript-run.txt" if kind == "elapsed" else "ruby-run.txt"
                write(self.dir, name, text)
                with self.assertRaises(report.ReportError) as ctx:
                    report.load_result(self.dir)
                self.assertIn(f"{name}:", str(ctx.exception))
                self.assertIn("more than once", str(ctx.exception))
                write(self.dir, "ruby-run.txt", RUBY_RUN)
                write(self.dir, "javascript-run.txt", JS_RUN)

    def test_missing_field(self) -> None:
        bad = RUBY_RUN.replace(" us_per_output=9.0", "", 1)
        write(self.dir, "ruby-run.txt", bad)
        with self.assertRaises(report.ReportError) as ctx:
            report.load_result(self.dir)
        self.assertIn("ruby-run.txt:5", str(ctx.exception))
        self.assertIn("us_per_output", str(ctx.exception))

    def test_crlf(self) -> None:
        names = ("ruby-run.txt", "javascript-run.txt", "ruby-digest.txt", "platform.txt")
        for name in names:
            text = (self.dir / name).read_bytes().replace(b"\n", b"\r\n")
            (self.dir / name).write_bytes(text)
        crlf = self.render()
        self.assertNotIn("\r", crlf)
        hand = HAND_TEXT.replace("\n", "\r\n")
        (self.dir / "README.md").write_bytes(hand.encode())
        self.assertEqual(run_main([str(self.dir)]), 0)
        data = (self.dir / "README.md").read_bytes().decode()
        before = hand[: hand.index(report.START_MARKER)]
        self.assertTrue(data.startswith(before + report.START_MARKER))
        self.assertTrue(data.endswith("| 3 | 4 |"))
        self.assertEqual(run_main([str(self.dir), "--check"]), 0)

    def test_check(self) -> None:
        self.assertEqual(run_main([str(self.dir), "--check"]), 1)
        self.assertEqual(run_main([str(self.dir)]), 0)
        self.assertEqual(run_main([str(self.dir), "--check"]), 0)
        self.assertTrue(report.check(self.dir))
        readme = self.dir / "README.md"
        readme.write_text(
            readme.read_text(encoding="utf-8").replace("Peak memory", "Peak"),
            encoding="utf-8",
        )
        self.assertEqual(run_main([str(self.dir), "--check"]), 1)
        self.assertFalse(report.check(self.dir))

    def test_committed_results_match(self) -> None:
        root = Path(__file__).resolve().parents[2] / "doc" / "benchmarks"
        folders = [
            readme.parent
            for readme in sorted(root.glob("*/README.md"))
            if any(
                marker in readme.read_text(encoding="utf-8")
                for marker in (report.START_MARKER, report.END_MARKER)
            )
        ]
        self.assertTrue(folders, "no committed result has the report markers")
        for folder in folders:
            with self.subTest(folder=folder.name):
                self.assertTrue(
                    report.check(folder),
                    f"{folder}: README.md differs from report.py output",
                )

    def test_note_explains_dash(self) -> None:
        out = self.render()
        self.assertIn("– means that the raw file has no record for the cell.", out)
        self.assertIn("An entry without a recursion edge has no recursive stage.", out)

    def test_no_run_or_digest_file(self) -> None:
        only = Path(self._tmp.name) / "only"
        only.mkdir()
        write(only, "platform.txt", PLATFORM)
        with self.assertRaises(report.ReportError) as ctx:
            report.load_result(only)
        self.assertIn("no run file and no digest file", str(ctx.exception))
        self.assertEqual(run_main([str(only)]), 1)
        self.assertFalse((only / "README.md").exists())

    def test_no_platform_file(self) -> None:
        (self.dir / "platform.txt").unlink()
        with self.assertRaises(report.ReportError) as ctx:
            report.load_result(self.dir)
        self.assertIn("no platform file", str(ctx.exception))
        self.assertEqual(run_main([str(self.dir)]), 1)

    def test_repeated_stage_and_trim(self) -> None:
        cases = {
            "stage": "stage entry=0 stage=rules steps=9 us_per_output=9.0"
            " avg_bytes=140 dup_pct=0.0 same_pct=0.0\n",
            "trim": "trim entry=244 steps=10 us_per_step=0.5\n",
        }
        for kind, line in cases.items():
            with self.subTest(kind=kind):
                write(self.dir, "ruby-run.txt", RUBY_RUN + line)
                with self.assertRaises(report.ReportError) as ctx:
                    report.load_result(self.dir)
                self.assertIn("ruby-run.txt:", str(ctx.exception))
                self.assertIn("more than once", str(ctx.exception))
                write(self.dir, "ruby-run.txt", RUBY_RUN)

    def test_end_marker_only(self) -> None:
        write(self.dir, "README.md", "text\n" + report.END_MARKER + "\n")
        with self.assertRaises(report.ReportError) as ctx:
            report.check(self.dir)
        self.assertIn(str(self.dir / "README.md"), str(ctx.exception))
        self.assertIn("exactly one start marker", str(ctx.exception))

    def test_mixed_platform_fields(self) -> None:
        for field in ("version", "system", "machine"):
            with self.subTest(field=field):
                old = {"version": "1.2.0", "system": "Darwin", "machine": "arm64"}[field]
                other = JS_RUN.replace(f"{field}={old}", f"{field}=other", 1)
                write(self.dir, "javascript-run.txt", other)
                with self.assertRaises(report.ReportError) as ctx:
                    report.load_result(self.dir)
                msg = str(ctx.exception)
                self.assertIn(field, msg)
                self.assertIn("javascript-run.txt", msg)
                self.assertIn("ruby-digest.txt", msg)
                write(self.dir, "javascript-run.txt", JS_RUN)

    def test_warning_without_optimization(self) -> None:
        self.assertNotIn("Warning:", self.render())
        slow = RUBY_RUN.replace("optimized=1", "optimized=0", 1)
        write(self.dir, "ruby-run.txt", slow)
        out = self.render()
        self.assertIn(
            "Warning: ruby-run.txt comes from a build without optimization.", out
        )
        self.assertLess(out.index("## Runs"), out.index("Warning:"))
        self.assertLess(out.index("Warning:"), out.index("## Time per output"))

    def test_help_and_unknown_option(self) -> None:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(report.main(["-h"]), 0)
            self.assertEqual(report.main(["--help"]), 0)
        self.assertIn("usage: report.py", out.getvalue())
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            self.assertEqual(report.main(["--bogus"]), 2)
            self.assertEqual(report.main([str(self.dir), "-x"]), 2)
        self.assertIn("usage: report.py", err.getvalue())

    @unittest.skipIf(
        not hasattr(os, "geteuid") or os.geteuid() == 0, "needs a non-root user"
    )
    def test_unreadable_readme(self) -> None:
        readme = self.dir / "README.md"
        write(self.dir, "README.md", HAND_TEXT)
        readme.chmod(0)
        self.addCleanup(readme.chmod, 0o644)
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            self.assertEqual(report.main([str(self.dir)]), 1)
            self.assertEqual(report.main([str(self.dir), "--check"]), 1)
        self.assertIn(f"report.py: {readme}:", err.getvalue())
        self.assertNotIn("Traceback", err.getvalue())

    def test_check_leaves_file_unchanged(self) -> None:
        readme = self.dir / "README.md"
        write(self.dir, "README.md", HAND_TEXT)
        before = readme.read_bytes()
        self.assertEqual(run_main([str(self.dir), "--check"]), 1)
        self.assertEqual(readme.read_bytes(), before)

    def test_empty_tables(self) -> None:
        (self.dir / "ruby-digest.txt").unlink()
        out = self.render()
        self.assertIn("## Digests\n\nNo digest in this result.\n", out)
        for name in ("ruby-run.txt", "javascript-run.txt"):
            (self.dir / name).unlink()
        write(self.dir, "ruby-digest.txt", RUBY_DIGEST)
        out = self.render()
        for heading in ("Time per output", "Duplicates", "Memory"):
            self.assertIn(f"## {heading}\n", out)
        self.assertEqual(out.count("No run in this result."), 3)

    def test_template_has_only_named_fields(self) -> None:
        import string

        names = [f[1] for f in string.Formatter().parse(report.REPORT) if f[1]]
        self.assertEqual(
            sorted(names),
            sorted(
                ["platform", "runs", "warnings", "times", "rates", "memory",
                 "digests", "note"]
            ),
        )

    def test_usage(self) -> None:
        self.assertEqual(run_main([]), 2)
        self.assertEqual(run_main(["a", "b", "c"]), 2)


if __name__ == "__main__":
    unittest.main()
