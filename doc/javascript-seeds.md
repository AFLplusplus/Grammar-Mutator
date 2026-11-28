JavaScript Seeds from Engine Test Suites
===

This document gives a recipe that makes JavaScript seeds from the test suites
of three JavaScript engines. It also gives the result of one run.

The seeds decide the result of a campaign more than the mutation operators
do. The grammar mutator cannot make only valid programs. So the seeds must
bring declared variables, real API calls, and code that the engine compiles.
Engine test suites are the best public source of such code.

In the recorded run, the recipe gives 68,370 seeds. The grammar parses 97.3%
of them, and JerryScript v3.0.0 runs 74.8% of them without an error.

## Quick Start

You need:

- `python3` (the standard library only).
- `git` 2.27 or later, and a network connection.
- About 3.3 GiB of disk space.
- A `.g4` build of the JavaScript grammar. A parse of one suite can use up
  to 7.1 GiB of memory.
- A JavaScript engine shell.

`DEST` is a work folder outside the repository. The commands use JerryScript;
see [Engines](#engines) for other engines.

```bash
TOOL=grammars/g4/javascript/seeds/get_seeds.py

# 1. Download the suites and write the seeds to DEST/seeds/<suite>/.
python3 $TOOL fetch DEST

# 2. Keep the seeds that the engine runs without an error.
python3 $TOOL filter --cmd 'jerry {shims} {seed}' DEST/seeds DEST/jerry

# 3. Build the grammar, and parse the kept seeds into trees.
make GRAMMAR_FILE="grammars/g4/javascript/JavaScriptLexer.g4 grammars/g4/javascript/JavaScriptParser.g4" \
     G4_SUPPORT_DIR=grammars/g4/javascript/Cpp
for suite in test262 mjsunit jstests; do
  ./grammar_parser-javascript DEST/jerry/$suite DEST/trees
done
python3 $TOOL count DEST/jerry DEST/trees

# 4. Fuzz.
mkdir -p out/default
cp -r DEST/trees out/default
export AFL_CUSTOM_MUTATOR_LIBRARY=./libgrammarmutator-javascript.so
export AFL_CUSTOM_MUTATOR_ONLY=1
afl-fuzz -i DEST/jerry -o out -- /path/to/jerry DEST/shims.js @@
```

Notes:

- Put `DEST/shims.js` before `@@` in the target command. The engine must run
  it before each seed (see [shims.js](#shimsjs)).
- AFL++ reads the suite folders below `-i`. The mutator reads the trees from
  `out/default/trees` (see
  [Copying the Trees](../README.md#copying-the-trees)).
- To measure the parse rate of all the seeds, parse `DEST/seeds/$suite`
  instead. The recorded run does this. 16,582 of its 66,511 trees belong to
  seeds that the filter drops.
- The next step is corpus minimization with `afl-cmin` and the instrumented
  engine. The recipe does not do this step.

## How It Works

### The Suites

| Suite name | Source | Folders |
|---|---|---|
| `test262` | [test262](https://github.com/tc39/test262), the conformance suite of the language | `test/annexB`, `built-ins`, `intl402`, `language`, `staging` |
| `mjsunit` | V8 | `test/mjsunit` |
| `jstests` | WebKit | `JSTests/stress`, `es6`, `ChakraCore`, `mozilla`, `regexp` |

`get_seeds.py` pins each suite to one commit (see
[Recorded Result](#recorded-result)).

### fetch

`fetch` downloads only the needed folders of each suite, at the pinned
commit. It keeps the download in `DEST/src/`. A second run with the same pins
does not download again. `DEST/seeds` must not exist, or it must be empty.

```
DEST/src/                               the downloaded suites
DEST/seeds/<suite>/<suite>_<path>.js    the seeds, one folder per suite
DEST/shims.js                           a copy of shims.js
DEST/summary.txt                        the pins, the counts, the drop reasons
```

The seed name is the path of the test file, with `/` and each unusual
character changed to `_`. AFL++ adds about 31 bytes to the name of an input
file. So the tool cuts a name longer than 200 bytes and adds a hash.

`fetch` drops these files. `summary.txt` counts each reason for each suite.

| Reason | Dropped file |
|---|---|
| `negative-parse`, `negative-resolution` | A test262 test that must not parse, or must not resolve |
| `module` | A test262 module test |
| `fixture` | A test262 module helper (`*_FIXTURE.js`) |
| `harness` | A helper file, not a test: `mjsunit.js`, and `shell.js` and `browser.js` of `JSTests/mozilla` |
| `resources` | A file in a JSTests `resources/` folder |
| `too-large` | A file larger than 1,048,576 bytes, the largest AFL++ input |
| `slow-parse` | A file that takes `grammar_parser-javascript` more than 60 s alone, with one thread (the `EXCLUDE` list of the tool) |

`fetch` changes the text of a test in two cases only:

- A test262 test with the `onlyStrict` flag gets `"use strict";` at the top,
  as the test262 runners do.
- A V8 runtime call, `%Name(`, becomes `__v8native("Name")(`. `shims.js`
  defines `__v8native`, and the arguments still run. `%` can also be the
  modulo operator. So the tool changes `%Name(` only where a call can start:
  - at the start of a line;
  - after one of the characters `( , = ; { } [ ! & | ? : + - >`;
  - after one of the keywords `return`, `typeof`, `void`, `throw`, `await`,
    `case`, `in`, `of`, `new`, `delete`, `else`, and `do`;
  - after the `)` of an `if`, `while`, or `for` head.

### shims.js

The test files call shell functions, such as `assertEquals`, `print`, `$vm`,
and `%OptimizeFunctionOnNextCall()`. Other engines do not have them. An
assert also throws when a mutation changes a value. An exception stops the
seed, often before the engine compiles the code with the JIT.

The engine runs `shims.js` first and then the seed, in one global context.
`shims.js` defines the shell and harness functions of the three suites:

| Rule | Example |
|---|---|
| An assert never throws. | `assertEquals(1, 2)` does nothing. |
| A callback or a string of code still runs, inside `try`/`catch`. | `assertThrows(fn)` calls `fn`. |
| A call on a shell object runs each function argument, never a string. `console` runs nothing. | `$vm.f(fn)` and `TemporalHelpers.check(fn)` call `fn`. |
| `shims.js` always replaces the assert functions. It defines another name only if the engine has no value for it. | `d8` and `jsc` keep their own helpers. |
| `shims.js` uses ES5 syntax only and declares nothing at the top level. | A seed can declare the same name with `let`, `class`, or `function`. |
| `shims.js` stays smaller than 10 KiB (10,240 bytes). | The engine parses the file on each run. |

Four shims do not run their code, because a real engine runs it in another
thread, realm, or module: `new Worker(code)`, `Realm.eval(realm, code)`,
`$262.agent.start(code)`, and `WScript.LoadModule(code)`. In the recorded
result, 197 kept seeds use one of them.

**The accepted gap.** A seed can define its own throwing helper. The
declaration of the seed replaces the shim, so a mutated value still throws:

```js
function shouldBe(actual, expected) {
  if (actual !== expected) throw new Error("bad value");
}
```

A seed can also throw inline, for example
`if (x !== 1) throw new Error("bad");`. Many JSTests `stress` files do one of
the two. The recipe does not change these seeds. A mutated seed of this kind
can stop early.

### filter

`filter` runs the `--cmd` command once for each seed. It changes `{shims}` to
the path of `shims.js` and `{seed}` to the path of the seed. It does not use
a shell. It copies a seed to the output folder, in its suite folder, if the
command ends with status 0 within the time limit.

| Option | Default | Meaning |
|---|---|---|
| `--cmd CMD` | (required) | The engine command. It must contain `{seed}`. |
| `--shims FILE` | `shims.js` next to the seed folder (`DEST/shims.js`) | The shims file. |
| `--timeout S` | 1 s, the default of AFL++ | The time limit of one run. |
| `-j N` | The number of CPUs | The number of runs at the same time. |

`filter` prints the kept, failed, timed-out, and crashed seeds of each suite,
and the name and signal of each crash. A crash is an engine bug or a problem
of `shims.js`. A loaded machine gives some time-outs, so the kept count can
change a little from one run to the next.

#### Engines

| Engine | `--cmd` | Target command of `afl-fuzz` |
|---|---|---|
| JerryScript | `jerry {shims} {seed}` | `jerry DEST/shims.js @@` |
| V8 | `d8 {shims} {seed}` | `d8 DEST/shims.js @@` |
| JavaScriptCore | `jsc {shims} {seed}` | `jsc DEST/shims.js @@` |
| SpiderMonkey | `js -f {shims} -f {seed}` | `js -f DEST/shims.js -f @@` |

Each of these shells runs its file arguments in order, in one global context.
A harness that runs inputs in one process must load `shims.js` itself.

### Parse

Run `grammar_parser-javascript` once for each suite, into one `trees`
folder. It keeps the trees that the folder already holds. One process for
each suite keeps the parser cache smaller. `count` prints the number of
seeds and trees of each suite.

`grammar_parser` has no time limit. Some large JSTests files parse slowly
when all the threads parse at the same time, because the threads share the
parser cache. In the recorded run, no new tree comes for up to 69 s. Each of
these files takes less than 15 s alone, and the run ends.

## Recorded Result

The run is on 2026-11-27. The trees come from the JavaScript grammar of the
commit that adds this document. `grammar_parser` gives the same trees on each
platform. The JerryScript column depends on the build below.

| Suite | Repository | Commit |
|---|---|---|
| test262 | `https://github.com/tc39/test262` | `4e4505c343d64b3c25da2338f2594504b0728594` |
| mjsunit | `https://github.com/v8/v8` | `1344a52aeaec26ca151586e9f8d08517e0997235` |
| jstests | `https://github.com/WebKit/WebKit` | `4940531db2fa181a710f9e204390628ab44e5183` |

| Suite | Files | Dropped (reason: count) | Seeds | Trees | Kept by JerryScript |
|---|---:|---|---:|---:|---:|
| test262 | 53,999 | negative-parse: 4,680; module: 604; fixture: 294; negative-resolution: 34; too-large: 1 | 48,386 | 46,852 (96.8%) | 37,199 (76.9%) |
| mjsunit | 9,995 | slow-parse: 3; harness: 1 | 9,991 | 9,872 (98.8%) | 7,192 (72.0%) |
| jstests | 10,231 | resources: 196; harness: 26; too-large: 14; slow-parse: 2 | 9,993 | 9,787 (97.9%) | 6,775 (67.8%) |
| total | 74,225 | 5,855 | 68,370 | 66,511 (97.3%) | 51,166 (74.8%) |

The two last columns are independent: 49,929 of the 51,166 kept seeds also
have a tree. A seed without a tree gets no grammar mutations.

| Suite | Seeds | Kept | Failed | Timeout | Crash |
|---|---:|---:|---:|---:|---:|
| test262 | 48,386 | 37,199 | 11,163 | 21 | 3 |
| mjsunit | 9,991 | 7,192 | 2,735 | 64 | 0 |
| jstests | 9,993 | 6,775 | 2,987 | 223 | 8 |
| total | 68,370 | 51,166 | 16,885 | 308 | 11 |

### Why Seeds Fail the Parse

| Suite | No tree | Main forms | Source of the counts |
|---|---:|---|---|
| test262 | 1,534 | Private names (`#x`, about 600), `import()` forms such as `import.source()`, `import.defer()`, and a second argument (217 in `dynamic-import`), trailing commas (142), `using` declarations (132), number literals such as `1..toString()` | Words in the file names |
| mjsunit | 119 | `using` declarations (24), private names (19) | A search of the file contents |
| jstests | 206 | 196 syntax errors, 10 files that are not UTF-8 | The messages of `grammar_parser-javascript` |

The `slow-parse` files of the `EXCLUDE` list:

| Suite | File | Parse time alone |
|---|---|---:|
| mjsunit | `es6/spread-array-misc.js` | 419 s |
| mjsunit | `regress/regress-818438.js` | more than 120 s |
| mjsunit | `regress/regress-crbug-1027025.js` | more than 120 s |
| jstests | `stress/array-flatten.js` | more than 120 s |
| jstests | `stress/v8-earley-boyer-strict.js` | more than 120 s |

All of them, except `v8-earley-boyer-strict.js`, have array elisions
(`[,,,]`).

### Why Seeds Fail the Filter

Most drops come from the engine, not from the recipe. test262 tests new parts
of the language, and JerryScript v3.0.0 does not have many of them.

A second run of the 17,204 dropped seeds, with the same build and time limit,
records the error of each seed. Each seed counts once, in the first row that
fits its error.

| Error | Seeds | Main cause |
|---|---:|---|
| `ReferenceError` for a global that JerryScript does not have | 8,986 | Built-ins such as `Temporal` (6,381), `Intl` (1,237), and `Iterator` (605), and the `WebAssembly` API (138) |
| `ReferenceError` for the V8 WebAssembly test library | 1,263 | `WasmModuleBuilder` and other names of this library |
| `ReferenceError` for other names | 1,222 | Harness names that fewer than 20 seeds miss, and a few other errors |
| `SyntaxError` | 1,966 | Regular expressions that the JerryScript regexp parser rejects (911), and `import.source()` or `import.defer()` (152) |
| `TypeError` | 2,144 | 1,093 of them are `Expected a function` |
| `RangeError` | 225 | A full stack (101), or no memory for an `ArrayBuffer` (48) or a `BigInt` (12) |
| Another thrown value | 665 | A check of the seed fails, for example `Error: Test failed` (see [the accepted gap](#shimsjs)) |
| Exit status 10 | 415 | The heap of 512 KB is full (jstests 246, mjsunit 159, test262 10) |
| Time-out | 297 | |
| Crash | 11 | See [Crashes](#crashes) |
| Exit status 0 | 10 | The seed passes in the second run |

`shims.js` defines each shell and harness name that 20 or more dropped seeds
miss, except the names in this table. These are the 10 names that the most
dropped seeds miss:

| Name | Seeds | Why `shims.js` does not define it |
|---|---:|---|
| `Temporal` | 6,381 | A built-in. A shim cannot add it. |
| `Intl` | 1,237 | A built-in. |
| `WasmModuleBuilder` | 1,192 | The V8 WebAssembly test library. JerryScript has no WebAssembly. |
| `Iterator` | 605 | A built-in. |
| `SIMD` | 204 | A built-in of a withdrawn proposal. |
| `WebAssembly` | 138 | The WebAssembly API. |
| `DisposableStack` | 98 | A built-in. |
| `FinalizationRegistry` | 88 | A built-in. |
| `AsyncDisposableStack` | 77 | A built-in. |
| `TZ_DIFF` | 76 | The date library of the Mozilla tests. It is too large for `shims.js`. |

Three more names are missing for the same reasons. `makeSig` comes from the
V8 WebAssembly test library. `generateBinaryTests` and `testCodePointRange`
take no callback: they hold the test code, so a shim that does nothing hides
that code.

### Crashes

The filter run gives these crashes of JerryScript v3.0.0. The list depends on
the build and on `shims.js`. Some crashes depend on the heap layout, so a
change of `shims.js` can show or hide them. JerryScript calls `abort()`
(SIGABRT) for a fatal error other than a full heap, for example the reference
count limit.

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

### Setup, Time, and Disk Space

JerryScript v3.0.0, a release build (`MinSizeRel`) with the default heap of
512 KB:

```bash
python3 tools/build.py --profile=es.next --lto=off --stack-limit=15 \
  --error-messages=on \
  --compile-flag=-Wno-enum-enum-conversion \
  --compile-flag=-Wno-unterminated-string-initialization
```

The two `--compile-flag` options are for Apple clang 21, which stops on two
warnings without them. Another compiler can need other flags, or none. The
filter uses the default time limit of 1 s.

The machine is an Apple M4 (10 cores: 4 performance and 6 efficiency) with
24 GiB of memory, on macOS 26.6.2. Other desktop programs also run on it. The
load average is between 2.0 and 3.9 at the start of the parse runs.

| Step | Time | Peak memory |
|---|---:|---:|
| `fetch`, first run (with the download) | 41.6 s | |
| `fetch`, second run (no download) | 11.8 s | |
| CMake build of the JavaScript grammar (`RelWithDebInfo`, 10 jobs) | 24.3 s | |
| `grammar_parser-javascript`, test262 (all seeds) | 495.5 s | 3.4 GiB |
| `grammar_parser-javascript`, mjsunit (all seeds) | 244.8 s | 5.7 GiB |
| `grammar_parser-javascript`, jstests (all seeds) | 532.5 s | 7.1 GiB |
| JerryScript build | 3.0 s | |
| `filter` with JerryScript, all suites (10 jobs) | 151.3 s | |

| Folder | Disk space (`du -sh`) |
|---|---:|
| `DEST/src` | 574 MiB |
| `DEST/seeds` | 326 MiB |
| `DEST/trees` (66,511 files, all seeds) | 2.2 GiB |
| `DEST/jerry` | 226 MiB |
| total | 3.3 GiB |
