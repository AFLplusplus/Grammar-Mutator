#!/usr/bin/env python
# -*- coding: utf-8 -*-

#
# american fuzzy lop++ - grammar mutator
# --------------------------------------
#
# Written by Shengtuo Hu
#
# Copyright 2020 AFLplusplus Project. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# A grammar-based custom mutator written for GSoC '20.
#

#
# The original version of this file is borrowed from F1 fuzzer:
# https://github.com/vrthra/F1
#
# We have made lots of changes to this file to satisfy our requirements.
#

import sys
import itertools
import random
import os
import string
import json

from f1_common import LimitFuzzer


class TreeNode:
    node_type: int = 0
    rule_id: int = 0
    val: str = ''
    parent: 'TreeNode' = None
    subnodes: list = None

    def __init__(self, node_type=0, rule_id=0, val='', subnodes=None):
        super().__init__()

        self.node_type = node_type
        self.rule_id = rule_id
        self.val = val

        if subnodes is None:
            subnodes = []
        self.subnodes = subnodes
        for subnode in self.subnodes:
            subnode.parent = self

    def __len__(self):
        return len(self.subnodes)

    def __setitem__(self, key, value):
        self.subnodes[key] = value

    def __getitem__(self, key):
        return self.subnodes[key]

    def append_subnode(self, subnode: 'TreeNode'):
        subnode.parent = self
        self.subnodes.append(subnode)

    def to_bytes(self):
        ret = bytes()

        # type
        ret += self.node_type.to_bytes(4, byteorder='little', signed=False)
        # rule id
        ret += self.rule_id.to_bytes(4, byteorder='little', signed=False)
        # subnode_count
        subnode_count = len(self)
        ret += subnode_count.to_bytes(4, byteorder='little', signed=False)
        # val_len
        val_len = len(self.val)
        ret += val_len.to_bytes(4, byteorder='little', signed=False)
        # val
        # Latin-1 is an 8-bit character set. The first 128 characters of its
        # set are identical to the US ASCII standard. By encoding the string as
        # Latin-1, we can handle all hex characters from \u0000 to \u00ff
        # Refs:
        # - https://stackoverflow.com/questions/66601743/python3-str-to-bytes-convertation-problem
        # - https://kb.iu.edu/d/aepu
        val_bytes = bytes(self.val, 'latin-1')
        if val_len != len(val_bytes):
            print(f'The length of `val` should be {val_len}, but found {len(val_bytes)}.')
            print(f'`val` bytes in UTF-8 encoding: {val_bytes}')
            print('Please check your grammar file!')
            sys.exit(1)
        ret += val_bytes

        # subnodes
        for subnode in self.subnodes:
            ret += subnode.to_bytes()

        return ret

    @staticmethod
    def from_bytes(data: bytes):
        node = TreeNode()
        consumed = 0

        # type
        node.node_type = int.from_bytes(data[consumed:consumed + 4], byteorder='little', signed=False)
        consumed += 4
        # rule id
        node.rule_id = int.from_bytes(data[consumed:consumed + 4], byteorder='little', signed=False)
        consumed += 4
        # subnode_count
        subnode_count = int.from_bytes(data[consumed:consumed + 4], byteorder='little', signed=False)
        consumed += 4
        # val_len
        val_len = int.from_bytes(data[consumed:consumed + 4], byteorder='little', signed=False)
        consumed += 4
        # val
        if val_len != 0:
            node.val = data[consumed:consumed + val_len].decode('utf-8')
        consumed += val_len

        # subnodes
        for _ in range(subnode_count):
            subnode, sub_consumed = TreeNode.from_bytes(data[consumed:])

            node.append_subnode(subnode)
            consumed += sub_consumed

        return node, consumed

    def __str__(self):
        ret = ''
        if len(self) == 0:
            if self.val is None or len(self.val) == 0:
                return ret
            ret += self.val
            return ret

        # subnodes
        for subnode in self.subnodes:
            ret += str(subnode)

        return ret


def bytes_to_c_str(data: bytes):
    return ''.join(["\\x%02X" % x for x in data])


class PooledFuzzer(LimitFuzzer):
    def __init__(self, grammar):
        super().__init__(grammar)
        self.c_grammar = self.cheap_grammar()
        self.c_grammar_keys = list(self.c_grammar.keys())

        self.MAX_SAMPLE = 255

        # reorder our grammar rules by cost.
        for k in self.grammar_keys:
            self.grammar[k] = [r for (i, r) in self.cost[k]]
        self.ordered_grammar = True

        self.pool_of_trees = self.completion_trees()

    def cheap_grammar(self):
        new_grammar = {}
        for k in self.grammar_keys:
            crules = self.cost[k]
            min_cost = crules[0][0]
            new_grammar[k] = [r for c, r in crules if c == min_cost]
            assert len(new_grammar[k]) > 0
        return new_grammar

    def k_to_id(self, k):
        return self.grammar_keys.index(k) + 1

    def get_trees_for_key(self, grammar, key='<start>'):
        '''
        For one key, generate a list of possible trees (one for each rule,
        downsampled to self.MAX_SAMPLE).
        '''
        # If this is a terminal node, just put in the one node:
        if key not in grammar:
            return [TreeNode(node_type=0, val=key)]

        # Enumerate the rules so we know how many there are and so we
        # can match rule IDs correctly:
        all_rules = list(enumerate(grammar[key]))

        # Generate trees for up to MAX_SAMPLE of them.
        # Each selected rule generates a list of trees (the subnodes of the rule), so this returns a list of lists.
        downsampled_rules = random.sample(all_rules, min(self.MAX_SAMPLE, len(all_rules)))
        return sum([self.get_trees_for_rule(grammar, key, rule_id, rule) for rule_id, rule in downsampled_rules],
                   [])

    def get_trees_for_rule(self, grammar, key, rule_id, rule):
        '''
        For each subnode of a rule, generate a list of all possible trees.
        '''
        assert grammar[key][rule_id] == rule
        # Make a list of possible child trees for each subnode of this rule (each will be maximum of MAX_SAMPLE long)
        # This is a list of lists, one list for each subnode.
        subnode_possibilities = [self.get_trees_for_key(grammar, key) for key in rule]

        # We want to randomly choose one option for each subnode to create a valid tree,
        # and we want a maximum of MAX_SAMPLE total trees:
        max_possible_trees = 1
        for subnode in subnode_possibilities:
            max_possible_trees *= len(subnode)

        # Clamp the possibilities to something sane:
        chosen_trees = min(max_possible_trees, self.MAX_SAMPLE)

        def random_product(*args, repeat=1):
            '''Pick ONE option from the equivalent of itertools.product()'''
            pools = [tuple(pool) for pool in args] * repeat
            return tuple(map(random.choice, pools))

        # Select chosen_trees number of random valid sets of subnodes, and return a tree for each
        return [TreeNode(node_type=self.k_to_id(key), rule_id=rule_id, subnodes=subnodes)
                for subnodes in (random_product(*subnode_possibilities) for _ in range(chosen_trees))]

    def completion_trees(self):
        return {k: self.get_trees_for_key(self.c_grammar, k)
                for k in self.c_grammar_keys}


# not clear what is the fastest: + or ''.join
# https://stackoverflow.com/questions/1316887/what-is-the-most-efficient-string-concatenation-method-in-python
class PyCompiledFuzzer(PooledFuzzer):
    def add_indent(self, string, indent):
        return '\n'.join([indent + i for i in string.split('\n')])

    # used for escaping inside strings
    def esc(self, t):
        t = t.replace('\\', '\\\\')
        t = t.replace('\n', '\\n')
        t = t.replace('\r', '\\r')
        t = t.replace('\t', '\\t')
        t = t.replace('\b', '\\b')
        t = t.replace('\v', '\\v')
        t = t.replace('"', '\\"')
        return t

    def esc_char(self, t):
        assert len(t) == 1
        t = t.replace('\\', '\\\\')
        t = t.replace('\n', '\\n')
        t = t.replace('\r', '\\r')
        t = t.replace('\t', '\\t')
        t = t.replace('\b', '\\b')
        t = t.replace('\v', '\\v')
        t = t.replace("'", "\\'")
        t = t.replace('"', '\\"')
        t = t.replace('\x00', '\\x00')  # null character
        return t

    def k_to_s(self, k):
        return k[1:-1].replace('-', '_')


class CFuzzer(PyCompiledFuzzer):
    def __init__(self, grammar):
        super().__init__(grammar)
        assert self.ordered_grammar

    def gen_rule_src(self, rule, key, min_rule_cost):
        res = []
        ntokens = len(rule)
        nkeys = len([token for token in rule if token in self.grammar])
        res.append('remaining_len = max_len - %d;' % min_rule_cost)
        res.append(
            'node->subnodes = (node_t**)malloc(%d * sizeof(node_t*));' % (
                ntokens))
        res.append('node->subnode_count = %d;' % ntokens)
        for i, token in enumerate(rule):
            if token in self.grammar:
                res.append('subnode_max_len = get_random_len(%d, remaining_len);' % nkeys)
                nkeys -= 1
                res.append('subnode_max_len += %d;' % self.key_cost[token])
                res.append('remaining_len += %d;' % self.key_cost[token])
                res.append('subnode = gen_node_%s(subnode_max_len, &subnode_consumed, -1);' % self.k_to_s(token))
                res.append('remaining_len -= subnode_consumed;')
                res.append('*consumed += subnode_consumed;')
                res.append('node->non_term_size += 1;')
                if key == token:
                    res.append('node->recursion_edge_size += 1;')
            else:
                esc_token_chars = [self.esc_char(c) for c in token]
                esc_token = ''.join(esc_token_chars)
                res.append(
                    'subnode = node_create_with_val(NODE_TERM__, "%s", %d);' % (
                        esc_token, len(esc_token_chars)))
                res.append('*consumed += %d;' % (len(token)))
            res.append('node->subnodes[%d] = subnode;' % i)
            res.append('subnode->parent = node;')
        return '\n    '.join(res)

    def gen_num_candidate_rules(self, k):
        min_rule_sizes = []
        num_min_rules = []
        for min_rule_size, _ in self.cost[k]:
            if min_rule_size not in min_rule_sizes:
                min_rule_sizes.append(min_rule_size)
                num_min_rules.append(0)
            num_min_rules[-1] += 1

        res = []
        num_candidate_rules = 0
        min_rule_sizes = min_rule_sizes[1:]
        for i, min_rule_size in enumerate(min_rule_sizes):
            num_candidate_rules += num_min_rules[i]
            if i == 0:
                res.append('if (max_len < %d) {' % min_rule_size)
            else:
                res.append('} else if (max_len < %d) {' % min_rule_size)
            res.append('  rules_that_fit = %d;' % num_candidate_rules)

        num_candidate_rules += num_min_rules[-1]
        if len(num_min_rules) == 1:
            res.append('rules_that_fit = %d;' % num_candidate_rules)
        else:
            res.append('} else {')
            res.append('  rules_that_fit = %d;' % num_candidate_rules)
            res.append('}')
        return '\n    '.join(res)

    def gen_alt_src_ser_tree(self, k):
        rules = self.grammar[k]
        cheap_trees = self.pool_of_trees[k]
        min_cost = self.cost[k][0][0]
        num_min_rules = len(self.c_grammar[k])
        result = list()
        result.append('''
node_t *gen_node_%(name)s(int max_len, int *consumed, int rule_index) {
  node_t *node = NULL;
  int val;

  if (max_len < %(min_cost)d) {
    val = map_rand(%(num_cheap_trees)d);
    size_t consumed = 0;
    const char* ser_data = pool_ser_%(name)s[val];
    const size_t ser_data_l = pool_l_ser_%(name)s[val];
    node = _node_deserialize((const uint8_t*)ser_data, ser_data_l, &consumed);
    return node;
  }

  if (rule_index < 0 || rule_index >= %(nrules)d) {
    int rules_that_fit = 0;
    %(gen_num_candidate_rules)s

    val = map_rand(rules_that_fit);
  } else {
    val = rule_index;
  }

  node = node_create_with_rule_id(NODE_%(node_type)s, val);

  *consumed = 0;
  int __attribute__((unused)) remaining_len = 0;
  int __attribute__((unused)) subnode_max_len = 0;
  int __attribute__((unused)) subnode_consumed = 0;

  node_t *subnode = NULL;
  switch(val) {''' % {
            'node_type': self.k_to_s(k).upper(),
            'name': self.k_to_s(k),
            'nrules': len(rules),
            'num_cheap_trees': len(cheap_trees),
            'min_cost': min_cost,
            'gen_num_candidate_rules': self.gen_num_candidate_rules(k)
        })

        for i, rule in enumerate(rules):
            result.append('''
  case %d:
    %s
    break;''' % (i, self.gen_rule_src(rule, k, self.cost[k][i][0])))

        result.append('''
  }

  return node;
}''')
        return '\n'.join(result)

    def ser_tree_pool_defs(self):
        result = []
        for k in self.grammar_keys:
            cheap_trees = self.pool_of_trees[k]
            ser_cheap_trees = [tree.to_bytes() for tree in cheap_trees]
            ser_cheap_trees_c_str = [bytes_to_c_str(ser_tree) for ser_tree in ser_cheap_trees]
            result.append('''
const char* pool_ser_%(k)s[] = {%(ser_trees)s};
const size_t pool_l_ser_%(k)s[] = {%(ser_trees_len)s};''' % {
                'k': self.k_to_s(k),
                'ser_trees': ', '.join(['"%s"' % ser_tree_c_str for ser_tree_c_str in ser_cheap_trees_c_str]),
                'ser_trees_len': ', '.join([str(len(ser_tree)) for ser_tree in ser_cheap_trees])})
        return '\n'.join(result)

    def fuzz_fn_decs(self):
        result = []
        for k in self.grammar_keys:
            result.append('''node_t *gen_node_%s(int max_len, int *consumed, int rule_index);''' % self.k_to_s(k))
        return '\n'.join(result)

    def fuzz_fn_defs(self):
        result = []
        for key in self.grammar:
            # result.append(self.gen_alt_src(key))
            # NOTE: use trees instead
            result.append(self.gen_alt_src_ser_tree(key))
        return '\n'.join(result)

    def node_type_decs(self):
        result = '''
enum node_type {
  NODE_TERM__ = 0,
  %s
};'''
        node_types = []
        for k in self.grammar_keys:
            node_types.append('NODE_%s,' % self.k_to_s(k).upper())
        return result % ('\n  '.join(node_types))

    def fuzz_fn_array_defs(self):
        result = '''
gen_func_t gen_funcs[%d] = {
  NULL,
  %s
};'''
        fuzz_fn_names = []
        for k in self.grammar_keys:
            fuzz_fn_names.append('gen_node_%s,' % self.k_to_s(k))
        return result % (
            len(self.grammar_keys) + 1,
            '\n  '.join(fuzz_fn_names))

    def node_type_str_defs(self):
        result = '''
const char *node_type_str(int node_type) {
  switch (node_type) {
  case 0: return "NODE_TERM__";
  %s
  default: return "";
  }
}'''
        node_type_strs = []
        for k in self.grammar_keys:
            node_type_strs.append('case %d: return "NODE_%s";' % (
                self.k_to_id(k), self.k_to_s(k).upper()))
        return result % ('\n  '.join(node_type_strs))

    def node_cost_array_defs(self):
        result = '''
size_t node_min_lens[%d] = {
  0,
  %s
};'''
        node_min_lens = []
        for k in self.grammar_keys:
            node_min_lens.append('%d,' % self.cost[k][0][0])
        return result % (
            len(self.grammar_keys) + 1,
            '\n  '.join(node_min_lens))

    def node_num_rules_array_defs(self):
        result = '''
size_t node_num_rules[%d] = {
  0,
  %s
};'''
        node_num_rules = []
        for k in self.grammar_keys:
            node_num_rules.append('%d,' % len(self.grammar[k]))
        return result % (
            len(self.grammar_keys) + 1,
            '\n  '.join(node_num_rules))

    def gen_fuzz_hdr(self):
        hdr_content = '''
#ifndef __F1_C_FUZZ_H__
#define __F1_C_FUZZ_H__

#include <stdint.h>

#include "tree.h"

#ifdef __cplusplus
extern "C" {
#endif

%(fuzz_fn_decs)s

tree_t *gen_init__(int max_len);

%(node_type_decs)s
const char *node_type_str(int node_type);

typedef node_t *(*gen_func_t)(int max_len, int *consumed, int rule_index);
extern gen_func_t gen_funcs[%(num_nodes)d];
extern size_t node_min_lens[%(num_nodes)d];
extern size_t node_num_rules[%(num_nodes)d];

#ifdef __cplusplus
}
#endif

#endif'''

        params = {
            "fuzz_fn_decs": self.fuzz_fn_decs(),
            "node_type_decs": self.node_type_decs(),
            "num_nodes": len(self.grammar_keys) + 1
        }

        return hdr_content % params

    def gen_fuzz_src(self):
        src_content = '''
#include <stdlib.h>
#include <string.h>

#include "tree.h"
#include "f1_c_fuzz.h"
#include "utils.h"

extern node_t *_node_deserialize(const uint8_t *data_buf,
                                 size_t data_size, size_t *consumed_size);

static inline int map_rand(int v) {
  return random_below(v);
}

static int get_random_len(int num_subnodes, int total_remaining_len) {
  int ret = total_remaining_len;
  int temp = 0;
  for (int i = 0; i < num_subnodes - 1; ++i) {
    temp = map_rand(total_remaining_len + 1);
    if (temp < ret) ret = temp;
  }
  return ret;
}
%(node_type_str_defs)s
%(ser_tree_pool_defs)s
%(fuzz_fn_defs)s
%(fuzz_fn_array_defs)s
%(node_cost_array_defs)s
%(node_num_rules_array_defs)s

tree_t *gen_init__(int max_len) {
  tree_t *tree = tree_create();
  int consumed = 0;
  tree->root = gen_funcs[1](max_len, &consumed, -1);
  return tree;
}'''

        params = {
            "ser_tree_pool_defs": self.ser_tree_pool_defs(),
            "fuzz_fn_defs": self.fuzz_fn_defs(),
            "fuzz_fn_array_defs": self.fuzz_fn_array_defs(),
            "node_type_str_defs": self.node_type_str_defs(),
            "node_cost_array_defs": self.node_cost_array_defs(),
            "node_num_rules_array_defs": self.node_num_rules_array_defs()
        }

        return src_content % params

    def fuzz_src(self):
        return self.gen_fuzz_hdr(), self.gen_fuzz_src()


def main(grammar, root_dir):
    random.seed(0)  # Fixed seed

    c_grammar = grammar

    hdr_path = os.path.join(root_dir, 'include/f1_c_fuzz.h')
    src_path = os.path.join(root_dir, 'src/f1_c_fuzz.c')
    fuzz_hdr, fuzz_src = CFuzzer(c_grammar).fuzz_src()
    with open(hdr_path, 'w') as f:
        print(fuzz_hdr, file=f)
    with open(src_path, 'w') as f:
        print(fuzz_src, file=f)


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(sys.argv[0] + ' </path/to/grammar/file> </path/to/output/dir>')
        sys.exit(1)

    grammar_file_path = sys.argv[1]
    with open(grammar_file_path, 'r') as fp:
        main(json.load(fp), sys.argv[2])
