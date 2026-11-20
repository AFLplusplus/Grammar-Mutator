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
"""Unit tests for grammars/g4_tree.py.

Run them with `make test-python`.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "grammars"))

from f1_c_gen import TreeNode
from g4_tree import RULE_ID_UNKNOWN, Node, deserialize, serialize, text


class TreeFileTest(unittest.TestCase):
    def test_serialize_matches_f1_c_gen(self) -> None:
        ref = TreeNode(3, 1, "", [TreeNode(0, 0, "a"), TreeNode(5, 0, "", [TreeNode(0, 0, "é")])])
        tree = Node(3, 1, "", [Node(0, 0, "a"), Node(5, 0, "", [Node(0, 0, "é")])])
        self.assertEqual(serialize(tree), ref.to_bytes())
        self.assertEqual(text(deserialize(serialize(tree))), "aé")

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


if __name__ == "__main__":
    unittest.main()
