#
# american fuzzy lop++ - grammar mutator
# --------------------------------------
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
"""Unit tests for grammars/tree_file.py.

Run them with `make test-python`.
"""

from __future__ import annotations

import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "grammars"))

from f1_c_gen import CFuzzer
from tree_file import RULE_ID_UNKNOWN, TERM_ID, Node, deserialize, serialize, text


class TreeFileTest(unittest.TestCase):
    def test_serialize_writes_the_c_format(self) -> None:
        # The format of _node_serialize in src/tree.c: id, rule_id,
        # subnode_count, val_len (u32 little-endian), then the value bytes
        tree = Node(3, 1, "", [Node(0, 0, "a"), Node(5, 0, "", [Node(0, 0, "é")])])
        expected = (
            struct.pack("<4I", 3, 1, 2, 0)
            + struct.pack("<4I", 0, 0, 0, 1)
            + b"a"
            + struct.pack("<4I", 5, 0, 1, 0)
            + struct.pack("<4I", 0, 0, 0, 2)
            + "é".encode()
        )
        self.assertEqual(serialize(tree), expected)
        self.assertEqual(text(deserialize(serialize(tree))), "aé")

    def test_f1_generator_trees_use_this_format(self) -> None:
        grammar = {"<start>": [["<a>", "<a>"]], "<a>": [["x"], ["y", "<a>"]]}
        fuzzer = CFuzzer(grammar)
        trees = fuzzer.completion_trees()["<start>"]
        self.assertTrue(trees)
        for tree in trees:
            self.assertIsInstance(tree, Node)
            self.assertEqual(deserialize(serialize(tree)), tree)
            self.assertEqual([n.id for n in tree.children], [fuzzer.k_to_id("<a>")] * 2)
            self.assertLessEqual(set(text(tree)), {"x", "y"})
            self.assertTrue(all(leaf.id == TERM_ID for child in tree.children for leaf in child.children[:1]))

    def test_unknown_rule_id_round_trips(self) -> None:
        tree = Node(2, RULE_ID_UNKNOWN, "", [Node(0, 0, "x")])
        self.assertEqual(deserialize(serialize(tree)).rule_id, RULE_ID_UNKNOWN)

    def test_deep_tree(self) -> None:
        root = node = Node(1, 0)
        for _ in range(100000):
            child = Node(1, 0)
            node.children.append(child)
            node = child
        node.children.append(Node(0, 0, "end"))
        self.assertEqual(text(deserialize(serialize(root))), "end")

    def test_round_trip_keeps_the_tree_shape(self) -> None:
        tree = Node(1, 2, "", [Node(0, 0, "a"), Node(3, RULE_ID_UNKNOWN, "", [Node(0, 0, "b")]), Node(4, 1)])
        self.assertEqual(deserialize(serialize(tree)), tree)
        self.assertNotEqual(deserialize(serialize(tree)), Node(1, 2, "", [Node(0, 0, "a")]))

    def test_deep_tree_compares_and_prints(self) -> None:
        def chain(leaf: str) -> Node:
            root = node = Node(1, 0)
            for _ in range(100000):
                child = Node(1, 0)
                node.children.append(child)
                node = child
            node.children.append(Node(0, 0, leaf))
            return root

        self.assertEqual(chain("x"), chain("x"))
        self.assertNotEqual(chain("x"), chain("y"))
        self.assertIn("children=1", repr(chain("x")))

    def test_non_utf8_leaf_bytes_round_trip(self) -> None:
        # A mutation in the C code can write any bytes into a leaf.
        data = serialize(Node(1, 0, "", [Node(0, 0, "a")])).replace(b"a", b"\xff")
        tree = deserialize(data)
        self.assertEqual(serialize(tree), data)

    def test_bad_tree_files_raise_value_error(self) -> None:
        data = serialize(Node(1, 0, "", [Node(0, 0, "ab")]))
        leaf_with_child = struct.pack("<8I", 0, 0, 1, 0, 0, 0, 0, 0)
        for bad in (b"", data[:10], data[:-1], data[:16], data + b"x", leaf_with_child):
            with self.assertRaises(ValueError, msg=bad):
                deserialize(bad)


if __name__ == "__main__":
    unittest.main()
