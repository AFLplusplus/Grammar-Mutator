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
