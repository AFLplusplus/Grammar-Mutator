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

You can give ANTLR 4 grammar files (`.g4`) instead of a JSON grammar, for
example from [grammars-v4](https://github.com/antlr/grammars-v4). The build
converts them with [g4_to_json.py](../grammars/g4_to_json.py). With make, run
`make deps` once for the Python tools.

```bash
make GRAMMAR_FILE=grammars/g4/JSON.g4
make GRAMMAR_FILE="grammars/g4/javascript/JavaScriptLexer.g4 grammars/g4/javascript/JavaScriptParser.g4"

# CMake: a list of full paths
cmake -DGRAMMAR_FILE="/path/JavaScriptLexer.g4;/path/JavaScriptParser.g4" ...
```

- Give all `.g4` files of the grammar, also the imported files.
- `GRAMMAR_START` sets the start rule. The default is the first parser rule.
- The grammar name comes from the last file without `Lexer` or `Parser`, so
  the example above gives `javascript`.

The generator ignores actions and predicates, so some generated test cases
can be invalid. It writes ASCII characters only, and it puts a space after
each token.

### Parse Existing Test Cases

The mutator parses each queue entry that has no tree, for example a seed or an
entry from `-M`/`-S` sync. This parse step can be slow for many large seeds.
To skip it, a `.g4` build also makes `grammar_parser-<name>`. This C program
parses your test cases and writes their trees. Copy the trees into the AFL++
output folder, as for `grammar_generator` (see
[README.md](../README.md#using-generated-seeds)).

```bash
./grammar_parser-javascript SEEDS_DIR TREES_DIR
mkdir -p out/default
cp -r TREES_DIR out/default/trees
```

- A tree gives the exact bytes of its test case again.
- The tree name is the file name, with each space changed to `_` (as AFL++
  does). `TREES_DIR` must not be in `SEEDS_DIR`.
- `-j N` sets the number of parser threads. The default is one per CPU. Put
  `-j N` before the folders.

The tool skips a file that it cannot parse, prints the reason, and goes on:

- `not UTF-8`: the file is not UTF-8.
- `syntax error`: ANTLR rejects the file.
- `parser limit`: the nesting is too deep (more than 200000 parser rule
  levels), or the grammar code throws an error.
- `duplicate name`: another file already has this tree name.
- `no match`: a bug. Please report it with the file.

If the mutator cannot parse a queue entry, the entry stays in the queue. The
AFL++ mutators still change it, but the custom mutator does not. The mutator
does not parse it again in the same run.

The mutator also stops a parse that takes too long. The time limit is 50 ×
the token count × the median time per token of the earlier parses. The
minimum is 50 ms. The limit starts after 32 successful parses. Set
`PARSE_TIME_BUDGET=0` to remove the limit. `grammar_parser-<name>` has no
time limit.

A grammar with action code or a `superClass` needs its C++ base classes. Set
`G4_SUPPORT_DIR` to the `Cpp/` folder of the grammar in grammars-v4 (CMake: a
full path). The `Python3/` folder does not work. Without the base classes, the
C++ compile step fails with a hint.

```bash
make GRAMMAR_FILE="grammars/g4/javascript/JavaScriptLexer.g4 grammars/g4/javascript/JavaScriptParser.g4" \
     G4_SUPPORT_DIR=grammars/g4/javascript/Cpp
```

### Limits

- A grammar must have no action code, or C++ base classes.
- Each token is one leaf, so the mutator changes a parsed token only as a
  whole.
- A token that only grammar code makes (such as `INDENT` in a Python grammar)
  is not supported.
