- `nautilus_py_grammars` directory contains grammar files used in [nautilus](https://github.com/nautilus-fuzz/nautilus/tree/master/grammars)
- `nautilus_py_grammar_to_json.py` can convert a nautilus grammar file to the JSON grammar format used in this project

    ```bash
    python nautilus_py_grammar_to_json.py /path/to/input/file /path/to/output/file
    ```

## Known Issues/Tips

### Avoid Left Recursion

As mentioned in [Issue #28](https://github.com/AFLplusplus/Grammar-Mutator/issues/28), using left recursion in the grammar file results in the wrong alternative numbers, which are used as "rule_id" in the grammar mutator. Simple grammars with left recursion can be converted to right recursion. Please try to avoid using left recursion for now.
