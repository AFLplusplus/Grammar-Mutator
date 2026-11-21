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
pass, and until it has one approving review.

Use one of these types:

| Type       | Use it for                                    | Release effect |
| ---------- | --------------------------------------------- | -------------- |
| `feat`     | A new feature                                 | Minor version  |
| `fix`      | A bug fix                                     | Patch version  |
| `perf`     | A performance improvement                     | Patch version  |
| `refactor` | A code change that keeps the behavior         | None           |
| `docs`     | Documentation only                            | None           |
| `test`     | Tests only                                    | None           |
| `build`    | The build system or dependencies              | None           |
| `ci`       | The CI configuration                          | None           |
| `chore`    | Other maintenance                             | None           |
| `style`    | Formatting only                               | None           |
| `revert`   | A revert of an earlier commit                 | Patch version  |

For a breaking change, add `!` after the type or scope, for example
`feat!: remove the JSON grammar format`. This gives a major version.

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
