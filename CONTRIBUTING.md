# Contributing to the Grammar Mutator

Thank you for your help. This file tells you how to send a change.

## Before you open a pull request

1. Build with the unit tests. See
   [building-grammar-mutator.md](doc/building-grammar-mutator.md).

   ```bash
   make ENABLE_TESTING=1 GRAMMAR_FILE=grammars/ruby.json
   ```

2. Run the unit tests and the memory check:

   ```bash
   make test
   make test_memcheck
   ```

3. If you change the Python tools in `grammars/` (for example the `.g4`
   tools), run their unit tests. They need java and the ANTLR jar.

   ```bash
   make test-python
   ```

   If you change the C++ matcher (`lib/antlr4_shim/g4_*`), build a `.g4`
   grammar and run the matcher tests. Run them with AddressSanitizer too. The
   ANTLR runtime has no ASan code, so the container check of ASan gives false
   reports. The script sets `ASAN_OPTIONS=detect_container_overflow=0` if
   `ASAN_OPTIONS` is empty. ASan cannot check a test that throws from more
   than 64 MB deep in the parse thread stack. An ASan build skips such a test
   and shows `[SKIP]` for it. The ASan run passes on macOS (Apple clang) and
   on Ubuntu 24.04 arm64 (gcc and clang). Other systems are not checked. CI
   has no ASan job.

   ```bash
   tests/g4/run_matcher_tests.sh
   MATCHER_FLAGS=-fsanitize=address tests/g4/run_matcher_tests.sh
   ```

4. Format the C and C++ code with clang-format 23. Other major versions give
   different output, so the format script refuses them. `make deps` installs
   clang-format 23 and the other Python tools into `.venv` with pip. The
   `format-check` CI job fails if `make code-format` changes a file.

   ```bash
   make deps   # once
   make code-format
   ```

5. Fill in the pull request template. The title must follow
   [Conventional Commits](#commit-messages-and-pull-request-titles).

## Measure a performance change

Use the benchmark program to measure a change that affects speed. The program
`benchmark-<name>` is in `src/benchmark/` after a make build, and in
`<build>/src/benchmark/` after a CMake build.

1. Build with optimization. Use make, or CMake with the default
   `RelWithDebInfo` build type.
2. Get seeds. For the JavaScript `.g4` grammar, download the JerryScript test
   files. For any grammar, use the generator.

   ```bash
   src/benchmark/get-jerryscript-seeds.sh <folder>
   grammar_generator-<name> <n> <size> <seeds> <trees> <random seed>
   ```

3. Run the program before and after the change. Use the same machine and the
   same options. Compare the `stage` and `trim` records.

   ```bash
   benchmark-<name> run <seed folder>
   ```

4. If the change must keep the outputs, compare the `digest` record before
   and after.

   ```bash
   benchmark-<name> digest <seed folder>
   ```

5. The program sets the step variables from `-n` and `-t`. It also turns the
   parse time limit off, unless you set `PARSE_TIME_BUDGET_MS`. So the same
   command does the same work.
6. To keep a result as a comparison point, follow
   [doc/benchmarks/README.md](doc/benchmarks/README.md).

## Branches

- `stable` is the only long-lived branch. All work goes into `stable`
  through pull requests.
- Do not push to `stable` directly.
- The `dev` branch is abandoned. It stays in the repository for history, but
  it does not get new changes. Do not open pull requests against `dev`.

## Branch names

Use [Conventional Branch](https://conventional-branch.github.io/) names. A
branch name has a prefix and a short description:

| Prefix                   | Use it for                         | Example                     |
| ------------------------ | ---------------------------------- | --------------------------- |
| `feature/` or `feat/`    | A new feature                      | `feat/g4-grammar-input`     |
| `bugfix/` or `fix/`      | A bug fix                          | `fix/trim-queue-new-entry`  |
| `hotfix/`                | An urgent fix                      | `hotfix/crash-on-empty-tree`|
| `release/`               | Release preparation                | `release/v1.2.0`            |
| `chore/`                 | Build, CI, or other maintenance    | `chore/release-please`      |
| `docs/`                  | Documentation only                 | `docs/contributing`         |

Rules for the description:

- Use lowercase letters (`a-z`), digits (`0-9`), and hyphens (`-`).
- Do not use two hyphens in a row. Do not start or end with a hyphen.
- You can use dots (`.`) only for a version in a `release/` branch.

CI does not check branch names. Please follow this rule all the same.

## Commit messages and pull request titles

Use [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/):

```
<type>[optional scope][!]: <description>
```

The repository uses squash merges only. The squash merge uses the pull request
title as the commit message on `stable`. So the **pull request title** must
follow Conventional Commits. The `PR Title` workflow checks it. A pull request
cannot merge until this check and the `make-build` and `cmake-build` CI jobs
pass, and until it has one approving review. A pull request that changes only
documentation (Markdown files, `doc/`, or the license) skips the CI jobs.

Use one of these types:

| Type       | Use it for                                    | Release effect |
| ---------- | --------------------------------------------- | -------------- |
| `feat`     | A new feature                                 | Minor version  |
| `fix`      | A bug fix                                     | Patch version  |
| `perf`     | A performance improvement                     | Patch version  |
| `refactor` | A code change that keeps the behavior         | Patch version  |
| `docs`     | Documentation only                            | Patch version  |
| `test`     | Tests only                                    | Patch version  |
| `build`    | The build system or dependencies              | Patch version  |
| `ci`       | The CI configuration                          | Patch version  |
| `chore`    | Other maintenance                             | Patch version  |
| `style`    | Formatting only                               | Patch version  |
| `revert`   | A revert of an earlier commit                 | Patch version  |

For a breaking change, add `!` after the type or scope, for example
`feat!: remove the JSON grammar format`. This gives a major version. The
release notes list every pull request, so each type gives at least a patch
version.

Examples:

```
feat: accept ANTLR .g4 grammar files
fix(trim): keep the current tree when AFL++ queues a case during trimming
docs: describe the TRIMMING_STEPS variable
```

Use the commit messages in your branch the same way. Keep the commit body
empty for most commits. Add one or two short sentences only when the reason
is not clear from the subject.

## Releases

[Release Please](https://github.com/googleapis/release-please) makes the
releases. Each merge into `stable` updates a release pull request. The release
pull request contains the next version and the changes for `CHANGELOG.md`.
When a maintainer merges it, Release Please tags the release (for example
`v1.1.0`) and publishes a GitHub release. GitHub Actions opens the release
pull request, so its workflows wait for a maintainer: click "Approve workflows
to run" before the merge, so that the required checks run.

The release notes list every merged pull request with its author, and the
new contributors. Release Please also writes the version into
`include/version.h`. Do not edit `CHANGELOG.md` or the version lines in
`include/version.h` by hand.
