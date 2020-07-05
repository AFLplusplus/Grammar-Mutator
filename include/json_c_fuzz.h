#ifndef __JSON_C_FUZZ_H__
#define __JSON_C_FUZZ_H__

#include "tree.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int max_depth;
node_t *   gen_start(int depth);
node_t *   gen_json(int depth);
node_t *   gen_element(int depth);
node_t *   gen_value(int depth);
node_t *   gen_object(int depth);
node_t *   gen_members(int depth);
node_t *   gen_member(int depth);
node_t *   gen_array(int depth);
node_t *   gen_elements(int depth);
node_t *   gen_string(int depth);
node_t *   gen_characters(int depth);
node_t *   gen_character(int depth);
node_t *   gen_esc(int depth);
node_t *   gen_escc(int depth);
node_t *   gen_number(int depth);
node_t *   gen_int(int depth);
node_t *   gen_digits(int depth);
node_t *   gen_digit(int depth);
node_t *   gen_onenine(int depth);
node_t *   gen_frac(int depth);
node_t *   gen_exp(int depth);
node_t *   gen_sign(int depth);
node_t *   gen_ws(int depth);
node_t *   gen_sp1(int depth);
node_t *   gen_symbol(int depth);
node_t *   gen_symbol_1(int depth);
node_t *   gen_symbol_2(int depth);
node_t *   gen_symbol_1_1(int depth);
node_t *   gen_character_1(int depth);
node_t *   gen_digit_1(int depth);

tree_t *gen_init__();

enum json_node_type {
  TERM_NODE = 0,  // the terminal node, a special type
  START,
  JSON,
  ELEMENT,
  VALUE,
  OBJECT,
  MEMBERS,
  MEMBER,
  ARRAY,
  ELEMENTS,
  STRING,
  CHARACTERS,
  CHARACTER,
  ESC,
  ESCC,
  NUMBER,
  INT,
  DIGITS,
  DIGIT,
  ONENINE,
  FRAC,
  EXP,
  SIGN,
  WS,
  SP1,
  SYMBOL,
  SYMBOL_1,
  SYMBOL_2,
  SYMBOL_1_1,
  CHARACTER_1,
  DIGIT_1
};

typedef node_t *(*gen_func_t)(int depth);
extern gen_func_t gen_funcs[31];

#ifdef __cplusplus
}
#endif

#endif
