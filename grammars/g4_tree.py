#!/usr/bin/env python3
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
"""Tree and tree file format for the grammars that g4_to_json.py reads.

A tree file holds one tree in pre-order. Each node stores these fields, all
as little-endian u32 values, and then its value bytes:

- id: the node type. A leaf has the id TERM_ID.
- rule_id: the alternative number. RULE_ID_UNKNOWN means the alternative is
  not known.
- subnode_count: the number of child nodes.
- val_len: the number of UTF-8 bytes in val.
- val: the value, as UTF-8 bytes.

The functions in this file use a loop, not recursion, so a deep tree works.
The format matches TreeNode.to_bytes in f1_c_gen.py.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field

TERM_ID = 0
RULE_ID_UNKNOWN = 0xFFFFFFFF

_HEADER = struct.Struct("<4I")


@dataclass
class Node:
    """A node of a parse tree."""

    id: int
    rule_id: int = 0
    val: str = ""
    children: list[Node] = field(default_factory=list)


def serialize(root: Node) -> bytes:
    """Return the tree file bytes of a tree."""
    out = bytearray()
    stack = [root]
    while stack:
        node = stack.pop()
        val = node.val.encode("utf-8")
        out += _HEADER.pack(node.id, node.rule_id, len(node.children), len(val))
        out += val
        # Push the children in reverse, so the first child is read next.
        stack.extend(reversed(node.children))
    return bytes(out)


def _read_node(data: bytes, pos: int) -> tuple[Node, int, int]:
    """Read one node header and value at pos. Return the node, the end position, and the child count."""
    node_id, rule_id, count, val_len = _HEADER.unpack_from(data, pos)
    start = pos + _HEADER.size
    end = start + val_len
    if end > len(data):
        raise ValueError("tree file ends inside a node value")
    return Node(node_id, rule_id, data[start:end].decode("utf-8")), end, count


def deserialize(data: bytes) -> Node:
    """Return the tree of tree file bytes."""
    root, pos, count = _read_node(data, 0)
    # Each open node has the number of child nodes that are still to read.
    open_nodes: list[Node] = []
    open_counts: list[int] = []
    if count:
        open_nodes.append(root)
        open_counts.append(count)
    while open_nodes:
        node, pos, count = _read_node(data, pos)
        open_nodes[-1].children.append(node)
        open_counts[-1] -= 1
        if count:
            open_nodes.append(node)
            open_counts.append(count)
        while open_nodes and open_counts[-1] == 0:
            open_nodes.pop()
            open_counts.pop()

    if pos != len(data):
        raise ValueError("tree file has bytes after the tree")
    return root


def text(root: Node) -> str:
    """Return the values of the leaves of a tree, in order."""
    parts: list[str] = []
    stack = [root]
    while stack:
        node = stack.pop()
        if node.children:
            stack.extend(reversed(node.children))
        else:
            parts.append(node.val)
    return "".join(parts)
