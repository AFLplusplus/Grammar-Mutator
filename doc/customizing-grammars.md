Customizing Grammars
===

The grammar file is specified in JSON and is shown as a collection of key/value pairs.
Each key represents a grammar token that is surrounded by angle brackets, and the corresponding value of a key is a list of grammar rules.
In the following case, `<START>` has two rules.

```json
{
    "<START>": [["RULE1"], ["RULE2"]]
}
```

For each grammar rule, it consist of a list of strings, which can either stand for a concrete string or a grammar token.

```json
["<A>", " likes ", "<B>"]
```

Given an input grammar, the grammar mutator constructs a tree representation for each input test case.
The tree is then turned into a concrete input for the target application.

Besides, we provide a script [nautilus_py_grammar_to_json.py](../grammars/nautilus_py_grammar_to_json.py) that can convert a nautilus grammar file to the JSON grammar format used in this project.
We plan to incorporate more features, such as using regex to specify grammar rule, in the future.

## Sample Grammar

Here is a very simple grammar file:

```json
{
    "<A>": [["I ", "<B>"]],
    "<B>": [["like ", "<C>"]],
    "<C>": [["C"], ["C++"]]
}
```

This simple grammar can generate two strings: "I like C" and "I like C++".

## ANTLR 4 Grammars (.g4)

You can also give one or more ANTLR 4 grammar files (`.g4`) instead of a JSON
grammar, for example a grammar from
[grammars-v4](https://github.com/antlr/grammars-v4). The build converts them
into a JSON grammar with [g4_to_json.py](../grammars/g4_to_json.py).

The `.g4` path needs the Python tools in `requirements.txt`. With make, run
`make deps` once to install them into `.venv`. With CMake, the configure step
installs them into the build folder.

```bash
# One combined grammar
make GRAMMAR_FILE=grammars/g4/JSON.g4

# A lexer grammar and a parser grammar
make GRAMMAR_FILE="grammars/g4/javascript/JavaScriptLexer.g4 grammars/g4/javascript/JavaScriptParser.g4"

# CMake: give a list of full paths
cmake -DGRAMMAR_FILE="/path/JavaScriptLexer.g4;/path/JavaScriptParser.g4" ...
```

- Give all `.g4` files of the grammar. If a grammar uses `import`, also give
  the imported files. The build stops if a rule or a token has no definition.
- `GRAMMAR_START` (CMake: `-DGRAMMAR_START`) is the start rule. The default is
  the first parser rule.
- The default grammar name comes from the last file, without `Parser` or
  `Lexer`. So the files above give `libgrammarmutator-javascript.so`.

The conversion works as follows:

- Each subrule and each `?`, `*`, and `+` becomes a helper rule.
- Each lexer rule becomes a rule over characters. A character set, a `~` set,
  or `.` gives one printable ASCII character, a tab, or a newline. A Unicode
  property such as `\p{L}` gives ASCII characters of the same kind (letters
  for `\p{L}`). A lexer rule that cannot give an ASCII character is removed.
- A literal with a non-ASCII character (for example `'\u200C'`) is removed,
  together with the alternative that contains it.
- A space comes after each token of a parser rule, because the lexer usually
  skips whitespace. A token that continues to the end of the line, such as a
  line comment, gets a newline after it.
- Actions, predicates, labels, options, and `EOF` are ignored. So a generated
  test case can break a rule that only a predicate or an action checks.
- The rules that the start rule cannot reach are removed.

### Parse Existing Test Cases

A `.g4` build also makes `grammar_parser-<name>`. It parses your own test cases
with the ANTLR parser of the grammar and writes a tree file for each one. Use it
like `grammar_generator-<name>`. Then copy the trees folder into the AFL++
output folder (see [README.md](../README.md#using-generated-seeds)).

```bash
./grammar_parser-javascript SEEDS_DIR TREES_DIR
mkdir -p out/default
cp -r TREES_DIR out/default/trees
afl-fuzz -i SEEDS_DIR -o out ...
```

- The tool reads each file in `SEEDS_DIR` and its subfolders. It writes
  `TREES_DIR/<file name>`, because the mutator finds a tree by the file name
  only. AFL++ changes each space in a queue file name to `_`. So the tool also
  changes each space in the tree file name to `_`. For example, the tree of
  `a b.js` is `a_b.js`.
- `TREES_DIR` must not be `SEEDS_DIR` or a folder in it.
- The tool parses the files in parallel, with one process for each CPU. Set
  the number of processes with `-j N`, for example `-j 1` for one process.
  The output does not depend on the number of processes.
- It prints one line for each skipped file and a summary with one count for
  each reason. The exit code is 0 also when it skips files. If `TREES_DIR`
  has a tree from an earlier run for a skipped file, the tool removes it.
- The exit code is 2 if the tool cannot read a file or write a tree, or if it
  cannot load the grammar or the parser.
- `grammar_parser-<name>` keeps its own copy of the grammar and the parser in
  `src/grammar_parser-<name>.d`. A later build of a different grammar does not
  change it.
- A tree gives the exact bytes of the test case again, together with the
  whitespace and the comments.
- Each token gets one leaf. So the mutator changes a parsed token only as a
  whole. For example, it does not change one character of a string literal.

The tool skips a file for one of these reasons:

- `not UTF-8`: the file is not valid UTF-8.
- `syntax error`: the ANTLR lexer or parser rejects the file.
- `parser limit`: the ANTLR runtime fails on the file, for example on very deep
  nesting. Or the tree is more than 20000 levels deep. The mutator walks a tree
  with recursion, and a much deeper tree can crash it. Each item of a list adds
  one level or more. So a file with 20000 statements gives such a deep tree.
- `duplicate name`: a file with the same tree name in a different folder
  already has a tree. For example, `a b.js` and `a_b.js` have the same tree
  name. Names that differ only in case also count as the same name, because
  the macOS file systems ignore case.
- `no match`: ANTLR accepts the file, but the parse tree does not match the
  converted grammar. This is usually a bug. Please report it with the file.

Many grammars from grammars-v4 have action code or predicates that call methods
of a base class, for example `{this.IsStrictMode()}?`. For these grammars, set
`G4_SUPPORT_DIR` (CMake: `-DG4_SUPPORT_DIR`) to the folder with the Python base
classes. This is the `Python3/` folder of the grammar in grammars-v4. If the
folder has `transformGrammar.py`, the build runs it on copies of the `.g4`
files. The build then copies the `.py` files next to the Python parser.

```bash
make GRAMMAR_FILE="grammars/g4/javascript/JavaScriptLexer.g4 grammars/g4/javascript/JavaScriptParser.g4" \
     G4_SUPPORT_DIR=grammars/g4/javascript/Python3

# CMake: give the full path
cmake -DGRAMMAR_FILE="/path/JavaScriptLexer.g4;/path/JavaScriptParser.g4" \
      -DG4_SUPPORT_DIR=/path/Python3 ...
```

CMake keeps `G4_SUPPORT_DIR` and `GRAMMAR_START` in its cache. A later JSON
grammar build in the same build folder ignores them and prints a warning.
Remove them with `-UG4_SUPPORT_DIR -UGRAMMAR_START`.

A grammar with no action code does not need `G4_SUPPORT_DIR`. If a grammar
needs it and you do not set it, `grammar_parser-<name>` stops with a message.
Generation and the mutator do not need `G4_SUPPORT_DIR`.

### Limits

- The mutator does not parse test cases with a `.g4` grammar itself. Use
  `grammar_parser-<name>` or `grammar_generator-<name>` to make the trees.
  The mutator still skips a queue entry that has no tree, for example an entry
  from a different fuzzer (`-M`/`-S` sync) or an entry that AFL++ trimmed.
  AFL++ fuzzes such an entry with its own mutators.
- A token that only the code of a grammar makes (a `tokens { ... }` entry with
  no lexer rule, such as `INDENT` in a Python grammar) cannot be generated.
  The build removes the alternatives that use it and prints a warning. For the
  same reason, `grammar_parser-<name>` does not support such grammars. A test
  case that uses such a token gives `no match`.
- Generated characters are ASCII only.
