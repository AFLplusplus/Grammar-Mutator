# Changelog

## 1.2.0 (2026-11-25)

<!-- Release notes generated using configuration in .github/release.yml at stable -->

## What's Changed
* perf: skip the AFL++ splice buffer by @h1994st in https://github.com/AFLplusplus/Grammar-Mutator/pull/75
* feat: name the mutation stage in queue file names by @h1994st in https://github.com/AFLplusplus/Grammar-Mutator/pull/77
* fix: keep the current tree when a new entry has no original file by @h1994st in https://github.com/AFLplusplus/Grammar-Mutator/pull/78
* fix: free the recursion edges that a trim step skips by @h1994st in https://github.com/AFLplusplus/Grammar-Mutator/pull/79
* perf: remove repeated tree copies and walks by @h1994st in https://github.com/AFLplusplus/Grammar-Mutator/pull/80
* fix: close the tree file on errors and fix remove_directory by @h1994st in https://github.com/AFLplusplus/Grammar-Mutator/pull/81
* fix: stop the parse hang of ambiguous grammars by @h1994st in https://github.com/AFLplusplus/Grammar-Mutator/pull/82


**Full Changelog**: https://github.com/AFLplusplus/Grammar-Mutator/compare/v1.1.0...v1.2.0

## 1.1.0 (2026-11-22)

<!-- Release notes generated using configuration in .github/release.yml at stable -->

## What's Changed
* ci: use GitHub release notes and add version.h by @h1994st in https://github.com/AFLplusplus/Grammar-Mutator/pull/70
* ci: skip the build jobs for documentation-only pull requests by @h1994st in https://github.com/AFLplusplus/Grammar-Mutator/pull/72
* docs: give the release effect of each commit type by @h1994st in https://github.com/AFLplusplus/Grammar-Mutator/pull/73
* feat: parse .g4 test cases with the ANTLR C++ runtime by @h1994st in https://github.com/AFLplusplus/Grammar-Mutator/pull/74


**Full Changelog**: https://github.com/AFLplusplus/Grammar-Mutator/compare/v1.0.0...v1.1.0

## 1.0.0 (2026-11-21)


### Features

* parse existing test cases with ANTLR 4 (.g4) grammars ([#67](https://github.com/AFLplusplus/Grammar-Mutator/issues/67)) ([29f3dd6](https://github.com/AFLplusplus/Grammar-Mutator/commit/29f3dd6aa6e686eb3e8a90e385a3cfbb5b162032))
* support ANTLR 4 (.g4) grammars for generation ([#66](https://github.com/AFLplusplus/Grammar-Mutator/issues/66)) ([7e54100](https://github.com/AFLplusplus/Grammar-Mutator/commit/7e54100149c969803a8aeeaf5e1f56d61133cf9c))
