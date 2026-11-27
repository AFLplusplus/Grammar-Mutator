JavaScript Seeds from Engine Test Suites
===

This document gives a recipe that makes JavaScript seeds from the test suites
of JavaScript engines. It also gives the result of one run of the recipe.

## What the Recipe Gives

The seeds decide the result of a fuzzing campaign more than the mutation
operators do. The grammar mutator cannot make only valid programs. So the
seeds must bring declared variables, real API calls, and code that the engine
compiles. The test suites of the engines are the best public source of such
code.

The recipe uses three suites, each at a pinned commit:

- [test262](https://github.com/tc39/test262), the conformance suite of the
  language.
- V8 `test/mjsunit`.
- WebKit `JSTests`: the folders `stress`, `es6`, `ChakraCore`, `mozilla`, and
  `regexp`.

The test files call shell functions, such as `assertEquals`, `print`, `$vm`,
and `%OptimizeFunctionOnNextCall()`. Other engines do not have them. An assert
also throws when a mutation changes a value. A seed that throws in its first
statement does not run the rest of its code. An exception also stops code
before the engine compiles it with the JIT.

The recipe solves this with one file, `shims.js`. The engine runs `shims.js`
first and then the seed, in one global context. `shims.js` defines the shell
functions and the harness functions of the three suites:

- An assert function never throws.
- A function that gets a callback or a string of code still runs it, inside
  `try`/`catch`. For example, `assertThrows(fn)` calls `fn`. A call on a
  shell object that has no fixed shim, such as `$vm` or `TemporalHelpers`,
  runs each function argument. It does not run a string, and `console` runs
  nothing. The shim catches each exception of the code, so the seed goes on.
- Four shims do not run their code: `new Worker(code)`,
  `Realm.eval(realm, code)`, `$262.agent.start(code)`, and
  `WScript.LoadModule(code)`. A real engine runs this code in another
  thread, realm, or module. In the recorded result, 196 kept seeds use one
  of them.
- `shims.js` always replaces the assert functions. It defines each other name
  only if the engine has no value for it. So `d8` and `jsc` keep their own
  functions.
- `shims.js` uses ES5 syntax only, so each engine can parse it. It declares
  nothing at the top level. A seed can declare the same name with `let`,
  `class`, or `function`.

The tool `grammars/g4/javascript/seeds/get_seeds.py` does the steps of the
recipe. It needs only Python 3 and `git`.

## Requirements

- `python3` (the standard library only).
- `git` 2.27 or later, and a network connection.
- About 3.2 GiB of disk space for the work folder (see
  [Time and Disk Space](#time-and-disk-space)).
- A `.g4` build of the JavaScript grammar, for the trees. A parse of one
  suite can use up to 7.1 GiB of memory.
- A JavaScript engine shell, for the filter.

## Steps

In the commands, `DEST` is a work folder outside the repository.

### 1. Download the Suites and Select the Seeds

```bash
python3 grammars/g4/javascript/seeds/get_seeds.py fetch DEST
```

The tool downloads only the needed folders of each suite, at the pinned
commit. It does not clone the full repository. It keeps the download in
`DEST/src/`. A second run with the same pins does not download again.
`DEST/seeds` must not exist, or it must be empty.

The tool writes:

```
DEST/seeds/<suite>/<suite>_<path>.js    the seeds, one folder per suite
DEST/shims.js                           a copy of shims.js
DEST/summary.txt                        the pins, the counts, the drop reasons
```

The suite names are `test262`, `mjsunit`, and `jstests`. The seed name is the
path of the test file, with `/` and each unusual character changed to `_`.

The tool drops some files. `summary.txt` counts each reason for each suite:

- `negative-parse`, `negative-resolution`: a test262 test that must not
  parse, or must not resolve.
- `module`: a test262 module test.
- `fixture`: a test262 module helper (`*_FIXTURE.js`).
- `harness`: a helper file of a suite, not a test (`mjsunit.js`, and the
  `shell.js` and `browser.js` files of `JSTests/mozilla`).
- `resources`: a file in a JSTests `resources/` folder.
- `too-large`: a file larger than 1,048,576 bytes, the largest AFL++ input.
- `slow-parse`: a file that takes `grammar_parser-javascript` more than 60 s
  to parse, alone and with one thread. The tool has a list of these files
  (`EXCLUDE`).

The tool changes the text of a test in two cases only:

- A V8 runtime call, `%Name(`, becomes `__v8native("Name")(`. `shims.js`
  defines `__v8native`. The arguments of the call still run.
- A test262 test with the `onlyStrict` flag gets `"use strict";` at the top,
  as the test262 runners do.

### 2. Parse the Seeds into Trees

Build the JavaScript grammar (see
[README.md](../README.md#building-the-grammar-mutator)):

```bash
make GRAMMAR_FILE="grammars/g4/javascript/JavaScriptLexer.g4 grammars/g4/javascript/JavaScriptParser.g4" \
     G4_SUPPORT_DIR=grammars/g4/javascript/Cpp
```

Run `grammar_parser-javascript` once for each suite. Use one `trees` folder
for all the suites. `grammar_parser-javascript` keeps the trees that the
folder already holds.
One process for each suite keeps the parser cache smaller.

```bash
for suite in test262 mjsunit jstests; do
  ./grammar_parser-javascript DEST/seeds/$suite DEST/trees
done
python3 grammars/g4/javascript/seeds/get_seeds.py count DEST/seeds DEST/trees
```

`count` prints the number of seeds and trees for each suite.

`grammar_parser` has no time limit. Some large JSTests files parse slowly
when all the threads parse at the same time, because the threads share the
parser cache. In the recorded run, `grammar_parser-javascript` writes no
new tree for up to 69 s. Each of these files takes less than 15 s alone, and the run ends.

### 3. Keep the Seeds That an Engine Runs

```bash
python3 grammars/g4/javascript/seeds/get_seeds.py filter \
  --cmd 'jerry {shims} {seed}' DEST/seeds DEST/jerry
```

The tool runs the command once for each seed. It changes `{shims}` to the
path of `shims.js`, and `{seed}` to the path of the seed. The default
`shims.js` is `DEST/shims.js`. Use `--shims` to give a different file. The
tool does not use a shell.

The tool copies a seed to the output folder if the command ends with status 0
within the time limit. The output folder keeps the suite folders. The tool
prints a table with the kept, failed, timed-out, and crashed seeds of each
suite. It also prints the name and the signal of each crash. A crash is an
engine bug or a problem of `shims.js`.

- `--timeout S` sets the time limit in seconds. The default is 1 s, the
  default time limit of AFL++.
- `-j N` sets the number of engine runs at the same time. The default is the
  number of CPUs.

Use the command line of your engine:

| Engine | `--cmd` | Target command of `afl-fuzz` |
|---|---|---|
| JerryScript | `jerry {shims} {seed}` | `jerry DEST/shims.js @@` |
| V8 | `d8 {shims} {seed}` | `d8 DEST/shims.js @@` |
| JavaScriptCore | `jsc {shims} {seed}` | `jsc DEST/shims.js @@` |
| SpiderMonkey | `js -f {shims} -f {seed}` | `js -f DEST/shims.js -f @@` |

Each of these shells runs its file arguments in order, in one global context.
A harness that runs inputs in one process must load `shims.js` itself.

### 4. Fuzz

Copy the trees into the output folder of `afl-fuzz` (see
[Copying the Trees](../README.md#copying-the-trees)). Give the kept seeds to
`afl-fuzz`. AFL++ reads the suite folders below `-i`. Put `shims.js` before
`@@` in the target command:

```bash
mkdir -p out/default
cp -r DEST/trees out/default
export AFL_CUSTOM_MUTATOR_LIBRARY=./libgrammarmutator-javascript.so
export AFL_CUSTOM_MUTATOR_ONLY=1
afl-fuzz -i DEST/jerry -o out -- /path/to/jerry DEST/shims.js @@
```

The `trees` folder can hold more trees than the kept seeds need. The mutator
reads only the trees of the queue entries.

The next step is corpus minimization. `afl-cmin` keeps a smaller set of seeds
with the same coverage. Run it with the instrumented engine and the same
target command. The recipe does not do this step.

## The Accepted Gap

A seed can define its own throwing helper, for example:

```js
function shouldBe(actual, expected) {
  if (actual !== expected) throw new Error("bad value");
}
```

The declaration of the seed replaces the shim. So a mutated value still
throws. A seed can also throw inline, for example
`if (x !== 1) throw new Error("bad");`. Many JSTests `stress` files do one of
the two. The recipe does not rename these helpers, and it does not remove
these checks. A mutated seed of this kind can stop early.

## Recorded Result

The result comes from one run on 2026-11-27, with the pins below. The trees
come from the JavaScript grammar of the commit that adds this document.
`grammar_parser` has no time limit and gives the same trees on each
platform. The JerryScript column depends on the JerryScript build below.

| Suite | Repository | Commit |
|---|---|---|
| test262 | `https://github.com/tc39/test262` | `4e4505c343d64b3c25da2338f2594504b0728594` |
| mjsunit | `https://github.com/v8/v8` | `1344a52aeaec26ca151586e9f8d08517e0997235` |
| jstests | `https://github.com/WebKit/WebKit` | `4940531db2fa181a710f9e204390628ab44e5183` |

| Suite | Files | Dropped (reason: count) | Seeds | Trees | Kept by JerryScript |
|---|---:|---|---:|---:|---:|
| test262 | 53,999 | negative-parse: 4,680; module: 604; fixture: 294; negative-resolution: 34; too-large: 1 | 48,386 | 46,852 | 37,200 |
| mjsunit | 9,995 | slow-parse: 3; harness: 1 | 9,991 | 9,735 | 7,089 |
| jstests | 10,231 | resources: 196; harness: 26; too-large: 14; slow-parse: 2 | 9,993 | 9,787 | 6,781 |
| total | 74,225 | 5,855 | 68,370 | 66,374 | 51,070 |

The grammar parses 97.1% of the seeds (test262 96.8%, mjsunit 97.4%,
jstests 97.9%). JerryScript keeps 74.7% of the seeds (test262 76.9%,
mjsunit 71.0%, jstests 67.9%). The two columns are independent: 49,833 of the
51,070 kept seeds also have a tree. A seed without a tree gets no grammar
mutations.

The filter result for each suite:

| Suite | Seeds | Kept | Failed | Timeout | Crash |
|---|---:|---:|---:|---:|---:|
| test262 | 48,386 | 37,200 | 11,164 | 19 | 3 |
| mjsunit | 9,991 | 7,089 | 2,838 | 64 | 0 |
| jstests | 9,993 | 6,781 | 2,987 | 217 | 8 |
| total | 68,370 | 51,070 | 16,989 | 300 | 11 |

The grammar rejects these main forms (counted from the file names):

- test262: private names (`#x`, about 600 files), `import()` forms such as
  `import.source()`, `import.defer()`, and a second argument (217 files in
  `dynamic-import`), `using` declarations (132), trailing commas (142), and
  number literals such as `1..toString()`.
- mjsunit: 256 files, most of them in `regress`, `compiler`, and `harmony`.
- jstests: 196 files with a syntax error and 10 files that are not UTF-8.

The `slow-parse` files are `mjsunit` `es6/spread-array-misc.js`,
`regress/regress-818438.js`, and `regress/regress-crbug-1027025.js`, and
`jstests` `stress/array-flatten.js` and `stress/v8-earley-boyer-strict.js`.
Alone, `spread-array-misc.js` takes 419 s. Each other file takes more than
120 s. All of them, except `v8-earley-boyer-strict.js`, have array elisions
(`[,,,]`).

### JerryScript Build

JerryScript v3.0.0, a release build (`MinSizeRel`) with the default heap of
512 KB:

```bash
python3 tools/build.py --profile=es.next --lto=off --stack-limit=15 \
  --error-messages=on \
  --compile-flag=-Wno-enum-enum-conversion \
  --compile-flag=-Wno-unterminated-string-initialization
```

The two `--compile-flag` options are for Apple clang 21. Without them, this
compiler stops on two warnings. Another compiler can need other flags, or
none.

The filter command and the time limit (1 s, the default):

```bash
python3 grammars/g4/javascript/seeds/get_seeds.py filter \
  --cmd '/path/to/jerryscript/build/bin/jerry {shims} {seed}' \
  DEST/seeds DEST/jerry
```

### Time and Disk Space

The machine is an Apple M4 (10 cores: 4 performance and 6 efficiency) with
24 GiB of memory, on macOS 26.6.2. Other desktop programs were open. The load
average was between 2.7 and 3.9 at the start of the parse runs.

| Step | Time | Peak memory |
|---|---:|---:|
| `fetch`, first run (with the download) | 41.6 s | |
| `fetch`, second run (no download) | 11.8 s | |
| CMake build of the JavaScript grammar (`RelWithDebInfo`, 10 jobs) | 24.3 s | |
| `grammar_parser-javascript`, test262 | 495.5 s | 3.4 GiB |
| `grammar_parser-javascript`, mjsunit | 254.3 s | 6.5 GiB |
| `grammar_parser-javascript`, jstests | 532.5 s | 7.1 GiB |
| JerryScript build | 3.0 s | |
| `filter` with JerryScript, all suites (10 jobs) | 150.1 s | |

The disk space of the work folder after all the steps is 3.2 GiB
(`du -sh`). `DEST/src` uses 574 MiB, `DEST/seeds` 326 MiB, `DEST/trees`
2.1 GiB (66,374 files), and `DEST/jerry` 225 MiB.

### Missing Names

A seed fails with `ReferenceError: <name> is not defined` if it uses a name
that neither JerryScript nor `shims.js` defines. These are the 10 names that
the most dropped seeds miss after the filter run:

| Name | Seeds | Source of the name |
|---|---:|---|
| `Temporal` | 6,381 | a built-in |
| `Intl` | 1,237 | a built-in |
| `WasmModuleBuilder` | 1,184 | the V8 WebAssembly test library |
| `Iterator` | 605 | a built-in |
| `SIMD` | 204 | a built-in of a withdrawn proposal |
| `WebAssembly` | 138 | the WebAssembly API |
| `DisposableStack` | 98 | a built-in |
| `FinalizationRegistry` | 88 | a built-in |
| `AsyncDisposableStack` | 77 | a built-in |
| `TZ_DIFF` | 76 | the date library of the Mozilla tests |

`shims.js` has each shell name and harness name that 20 or more dropped
seeds miss, with these exceptions:

- JavaScript built-ins that JerryScript does not have, such as `Temporal`,
  `Intl`, and `Iterator`. A shim cannot add them.
- `WasmModuleBuilder` and `makeSig`. They come from the V8 WebAssembly test
  library, and JerryScript has no WebAssembly.
- `TZ_DIFF`. It is the first name of the date library of the Mozilla tests.
  This library is too large for `shims.js`.
- `generateBinaryTests` and `testCodePointRange`. They take no callback.
  `generateBinaryTests` makes the test functions from strings, and
  `testCodePointRange` holds the test loop. A shim that does nothing hides
  that code.

415 dropped seeds (247 jstests, 158 mjsunit, and 10 test262 seeds) end with
exit status 10. JerryScript uses this status when its heap of 512 KB is full.

### Crashes

The filter run gives these crashes of JerryScript v3.0.0. The list depends on
the build and on `shims.js`. Some crashes depend on the heap layout, so a
change of `shims.js` can show or hide them.

| Seed | Signal |
|---|---|
| `test262_staging_sm_BigInt_large-bit-length.js` | SIGBUS |
| `test262_staging_sm_RegExp_replace-twoBytes.js` | SIGSEGV |
| `test262_staging_sm_String_replace-bad-dollar-single-quote.js` | SIGSEGV |
| `jstests_stress_array-prototype-concat-of-long-spliced-arrays.js` | SIGABRT |
| `jstests_stress_array-prototype-concat-of-long-spliced-arrays2.js` | SIGABRT |
| `jstests_stress_promise-capability-fast-path.js` | SIGABRT |
| `jstests_stress_promise-capability-slow-path.js` | SIGABRT |
| `jstests_stress_promise-capability-then-slow-path.js` | SIGABRT |
| `jstests_stress_promise-resolve-reject-cross-realm-dfg.js` | SIGABRT |
| `jstests_stress_promise-resolve-subclass-not-identity.js` | SIGABRT |
| `jstests_stress_promise-then-cross-realm-dfg.js` | SIGABRT |

JerryScript calls `abort()` (SIGABRT) for a fatal error other than a full
heap, for example the reference count limit.
