- `nautilus_py_grammars` directory contains grammar files used in [nautilus](https://github.com/nautilus-fuzz/nautilus/tree/master/grammars)
- `nautilus_py_grammar_to_json.py` can convert a nautilus grammar file to the JSON grammar format used in this project

    ```bash
    python nautilus_py_grammar_to_json.py /path/to/input/file /path/to/output/file
    ```

## Known Issues/Tips

### Avoid Left Recursion

As mentioned in [Issue #28](https://github.com/AFLplusplus/Grammar-Mutator/issues/28), using left recursion in the grammar file results in the wrong alternative numbers, which are used as "rule_id" in the grammar mutator. Simple grammars with left recursion can be converted to right recursion. Please try to avoid using left recursion for now.

### Hex Character

As mentioned in [Issue #29](https://github.com/AFLplusplus/Grammar-Mutator/issues/29), JSON does not support hex escape like `\xNN` (see more details [here](https://www.json.org/json-en.html)). To embed hex characters in the grammar file, one workaround is to use the Unicode escape in JSON by converting `\xNN` to `\u00NN`.

An example, `test_hex.json`, is included in this directory:

```json
{
    "<start>": [["hex: ", "<hex>"]],
    "<hex>": [["\u0000"], ["\u0001"], ["\u0002"], ["\u0003"], ["\u0004"], ["\u0005"], ["\u0006"], ["\u0007"],
              ["\u0008"], ["\u0009"], ["\u000a"], ["\u000b"], ["\u000c"], ["\u000d"], ["\u000e"], ["\u000f"],
              ["\u0010"], ["\u0011"], ["\u0012"], ["\u0013"], ["\u0014"], ["\u0015"], ["\u0016"], ["\u0017"],
              ["\u0018"], ["\u0019"], ["\u001a"], ["\u001b"], ["\u001c"], ["\u001d"], ["\u001e"], ["\u001f"]]
}
```

Note that, this workaround only works for ASCII characters (i.e., `\u0000` \~ `\u00ff`). Otherwise, the grammar file cannot be processed.

References: [post1](https://www.utf8-chartable.de/), [post2](https://stackoverflow.com/a/59624562), [post3](https://stackoverflow.com/a/66601996), and [post4](https://stackoverflow.com/questions/66601743/python3-str-to-bytes-convertation-problem).
