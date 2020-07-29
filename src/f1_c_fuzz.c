
#include <stdlib.h>
#include <string.h>

#include "tree.h"
#include "f1_c_fuzz.h"

int max_depth = -1;

static inline int map_rand(int v) {
  return random() % v;
}

const char* pool_start[] =  {"null", "false", "true"};
const int pool_l_start[] =  {4, 5, 4};

const char* pool_json[] =  {"false", "true", "null"};
const int pool_l_json[] =  {5, 4, 4};

const char* pool_element[] =  {"true", "null", "false"};
const int pool_l_element[] =  {4, 4, 5};

const char* pool_value[] =  {"false", "null", "true"};
const int pool_l_value[] =  {5, 4, 4};

const char* pool_object[] =  {"{}"};
const int pool_l_object[] =  {2};

const char* pool_members[] =  {"\"\":false", "\"\":null", "\"\":true"};
const int pool_l_members[] =  {8, 7, 7};

const char* pool_member[] =  {"\"\":null", "\"\":false", "\"\":true"};
const int pool_l_member[] =  {7, 8, 7};

const char* pool_array[] =  {"[]"};
const int pool_l_array[] =  {2};

const char* pool_elements[] =  {"null", "false", "true"};
const int pool_l_elements[] =  {4, 5, 4};

const char* pool_string[] =  {"\"\""};
const int pool_l_string[] =  {2};

const char* pool_characters[] =  {""};
const int pool_l_characters[] =  {0};

const char* pool_character[] =  {"l", "F", "O", "S", "h", "1", "}", "/", "_", "?", "&", "H", "7", "d", "*", "G", "T", "K", "o", "V", ".", "-", "v", "^", "L", "f", "0", "z", "w", "a", "<", "\"", "4", "6", "P", "e", "p", "+", "I", "E", "9", "n", "W", "%", "$", "R", "i", "b", "#", "U", " ", "t", ":", "D", "B", "3", "=", "@", "j", "`", "[", "8", "(", ")", "x", "Z", "A", "C", "J", ",", "M", "c", "m", "y", "N", "~", "r", "q", "2", "Q", "g", "X", "5", "|", "]", "!", "u", "Y", ";", ">", "{", "s", "k"};
const int pool_l_character[] =  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

const char* pool_esc[] =  {"\\n", "\\f", "\\r", "\\b", "\\t", "\\\"", "\\\\"};
const int pool_l_esc[] =  {2, 2, 2, 2, 2, 2, 2};

const char* pool_escc[] =  {"t", "\\", "r", "f", "n", "b", "\""};
const int pool_l_escc[] =  {1, 1, 1, 1, 1, 1, 1};

const char* pool_number[] =  {"0"};
const int pool_l_number[] =  {1};

const char* pool_int[] =  {"0"};
const int pool_l_int[] =  {1};

const char* pool_digits[] =  {"0"};
const int pool_l_digits[] =  {1};

const char* pool_digit[] =  {"0"};
const int pool_l_digit[] =  {1};

const char* pool_onenine[] =  {"7", "3", "4", "5", "1", "2", "9", "8", "6"};
const int pool_l_onenine[] =  {1, 1, 1, 1, 1, 1, 1, 1, 1};

const char* pool_frac[] =  {""};
const int pool_l_frac[] =  {0};

const char* pool_exp[] =  {""};
const int pool_l_exp[] =  {0};

const char* pool_sign[] =  {"", "-", "+"};
const int pool_l_sign[] =  {0, 1, 1};

const char* pool_ws[] =  {""};
const int pool_l_ws[] =  {0};

const char* pool_sp1[] =  {"\t", " ", "\n", "\r"};
const int pool_l_sp1[] =  {1, 1, 1, 1};

const char* pool_symbol[] =  {",\"\":null", ",\"\":false", ",\"\":true"};
const int pool_l_symbol[] =  {8, 9, 8};

const char* pool_symbol_1[] =  {",null", ",true", ",false"};
const int pool_l_symbol_1[] =  {5, 5, 6};

const char* pool_symbol_2[] =  {""};
const int pool_l_symbol_2[] =  {0};

const char* pool_symbol_1_1[] =  {""};
const int pool_l_symbol_1_1[] =  {0};

const char* pool_character_1[] =  {""};
const int pool_l_character_1[] =  {0};

const char* pool_digit_1[] =  {"0"};
const int pool_l_digit_1[] =  {1};


node_t *gen_start(int depth) {
  node_t *node = node_create(START);

  if (depth > max_depth) {
    int val = map_rand(3);
    const char* str = pool_start[val];
    const int str_l = pool_l_start[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(1);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = gen_json(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    break;

  }

  return node;
}

node_t *gen_json(int depth) {
  node_t *node = node_create(JSON);

  if (depth > max_depth) {
    int val = map_rand(3);
    const char* str = pool_json[val];
    const int str_l = pool_l_json[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(1);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = gen_element(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    break;

  }

  return node;
}

node_t *gen_element(int depth) {
  node_t *node = node_create(ELEMENT);

  if (depth > max_depth) {
    int val = map_rand(3);
    const char* str = pool_element[val];
    const int str_l = pool_l_element[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(1);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(3 * sizeof(node_t*));
    node->subnode_count = 3;
    subnode = gen_ws(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    subnode = gen_value(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    subnode = gen_ws(depth +1);
    node->non_term_size += 1;
    node->subnodes[2] = subnode;
    break;

  }

  return node;
}

node_t *gen_value(int depth) {
  node_t *node = node_create(VALUE);

  if (depth > max_depth) {
    int val = map_rand(3);
    const char* str = pool_value[val];
    const int str_l = pool_l_value[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(7);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "false", 5);
    node->subnodes[0] = subnode;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "null", 4);
    node->subnodes[0] = subnode;
    break;

  case 2:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "true", 4);
    node->subnodes[0] = subnode;
    break;

  case 3:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = gen_array(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    break;

  case 4:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = gen_object(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    break;

  case 5:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = gen_number(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    break;

  case 6:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = gen_string(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    break;

  }

  return node;
}

node_t *gen_object(int depth) {
  node_t *node = node_create(OBJECT);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_object[val];
    const int str_l = pool_l_object[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(2);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(3 * sizeof(node_t*));
    node->subnode_count = 3;
    subnode = node_create_with_val(TERM_NODE, "{", 1);
    node->subnodes[0] = subnode;
    subnode = gen_ws(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    subnode = node_create_with_val(TERM_NODE, "}", 1);
    node->subnodes[2] = subnode;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(3 * sizeof(node_t*));
    node->subnode_count = 3;
    subnode = node_create_with_val(TERM_NODE, "{", 1);
    node->subnodes[0] = subnode;
    subnode = gen_members(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    subnode = node_create_with_val(TERM_NODE, "}", 1);
    node->subnodes[2] = subnode;
    break;

  }

  return node;
}

node_t *gen_members(int depth) {
  node_t *node = node_create(MEMBERS);

  if (depth > max_depth) {
    int val = map_rand(3);
    const char* str = pool_members[val];
    const int str_l = pool_l_members[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(1);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(2 * sizeof(node_t*));
    node->subnode_count = 2;
    subnode = gen_member(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    subnode = gen_symbol_2(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    break;

  }

  return node;
}

node_t *gen_member(int depth) {
  node_t *node = node_create(MEMBER);

  if (depth > max_depth) {
    int val = map_rand(3);
    const char* str = pool_member[val];
    const int str_l = pool_l_member[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(1);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(5 * sizeof(node_t*));
    node->subnode_count = 5;
    subnode = gen_ws(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    subnode = gen_string(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    subnode = gen_ws(depth +1);
    node->non_term_size += 1;
    node->subnodes[2] = subnode;
    subnode = node_create_with_val(TERM_NODE, ":", 1);
    node->subnodes[3] = subnode;
    subnode = gen_element(depth +1);
    node->non_term_size += 1;
    node->subnodes[4] = subnode;
    break;

  }

  return node;
}

node_t *gen_array(int depth) {
  node_t *node = node_create(ARRAY);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_array[val];
    const int str_l = pool_l_array[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(2);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(3 * sizeof(node_t*));
    node->subnode_count = 3;
    subnode = node_create_with_val(TERM_NODE, "[", 1);
    node->subnodes[0] = subnode;
    subnode = gen_ws(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    subnode = node_create_with_val(TERM_NODE, "]", 1);
    node->subnodes[2] = subnode;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(3 * sizeof(node_t*));
    node->subnode_count = 3;
    subnode = node_create_with_val(TERM_NODE, "[", 1);
    node->subnodes[0] = subnode;
    subnode = gen_elements(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    subnode = node_create_with_val(TERM_NODE, "]", 1);
    node->subnodes[2] = subnode;
    break;

  }

  return node;
}

node_t *gen_elements(int depth) {
  node_t *node = node_create(ELEMENTS);

  if (depth > max_depth) {
    int val = map_rand(3);
    const char* str = pool_elements[val];
    const int str_l = pool_l_elements[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(1);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(2 * sizeof(node_t*));
    node->subnode_count = 2;
    subnode = gen_element(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    subnode = gen_symbol_1_1(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    break;

  }

  return node;
}

node_t *gen_string(int depth) {
  node_t *node = node_create(STRING);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_string[val];
    const int str_l = pool_l_string[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(1);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(3 * sizeof(node_t*));
    node->subnode_count = 3;
    subnode = node_create_with_val(TERM_NODE, "\"", 1);
    node->subnodes[0] = subnode;
    subnode = gen_characters(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    subnode = node_create_with_val(TERM_NODE, "\"", 1);
    node->subnodes[2] = subnode;
    break;

  }

  return node;
}

node_t *gen_characters(int depth) {
  node_t *node = node_create(CHARACTERS);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_characters[val];
    const int str_l = pool_l_characters[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(1);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = gen_character_1(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    break;

  }

  return node;
}

node_t *gen_character(int depth) {
  node_t *node = node_create(CHARACTER);

  if (depth > max_depth) {
    int val = map_rand(93);
    const char* str = pool_character[val];
    const int str_l = pool_l_character[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(94);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, " ", 1);
    node->subnodes[0] = subnode;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "!", 1);
    node->subnodes[0] = subnode;
    break;

  case 2:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "\"", 1);
    node->subnodes[0] = subnode;
    break;

  case 3:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "#", 1);
    node->subnodes[0] = subnode;
    break;

  case 4:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "$", 1);
    node->subnodes[0] = subnode;
    break;

  case 5:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "%", 1);
    node->subnodes[0] = subnode;
    break;

  case 6:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "&", 1);
    node->subnodes[0] = subnode;
    break;

  case 7:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "(", 1);
    node->subnodes[0] = subnode;
    break;

  case 8:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, ")", 1);
    node->subnodes[0] = subnode;
    break;

  case 9:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "*", 1);
    node->subnodes[0] = subnode;
    break;

  case 10:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "+", 1);
    node->subnodes[0] = subnode;
    break;

  case 11:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, ",", 1);
    node->subnodes[0] = subnode;
    break;

  case 12:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "-", 1);
    node->subnodes[0] = subnode;
    break;

  case 13:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, ".", 1);
    node->subnodes[0] = subnode;
    break;

  case 14:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "/", 1);
    node->subnodes[0] = subnode;
    break;

  case 15:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "0", 1);
    node->subnodes[0] = subnode;
    break;

  case 16:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "1", 1);
    node->subnodes[0] = subnode;
    break;

  case 17:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "2", 1);
    node->subnodes[0] = subnode;
    break;

  case 18:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "3", 1);
    node->subnodes[0] = subnode;
    break;

  case 19:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "4", 1);
    node->subnodes[0] = subnode;
    break;

  case 20:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "5", 1);
    node->subnodes[0] = subnode;
    break;

  case 21:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "6", 1);
    node->subnodes[0] = subnode;
    break;

  case 22:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "7", 1);
    node->subnodes[0] = subnode;
    break;

  case 23:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "8", 1);
    node->subnodes[0] = subnode;
    break;

  case 24:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "9", 1);
    node->subnodes[0] = subnode;
    break;

  case 25:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, ":", 1);
    node->subnodes[0] = subnode;
    break;

  case 26:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, ";", 1);
    node->subnodes[0] = subnode;
    break;

  case 27:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "<", 1);
    node->subnodes[0] = subnode;
    break;

  case 28:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "=", 1);
    node->subnodes[0] = subnode;
    break;

  case 29:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, ">", 1);
    node->subnodes[0] = subnode;
    break;

  case 30:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "?", 1);
    node->subnodes[0] = subnode;
    break;

  case 31:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "@", 1);
    node->subnodes[0] = subnode;
    break;

  case 32:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "A", 1);
    node->subnodes[0] = subnode;
    break;

  case 33:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "B", 1);
    node->subnodes[0] = subnode;
    break;

  case 34:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "C", 1);
    node->subnodes[0] = subnode;
    break;

  case 35:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "D", 1);
    node->subnodes[0] = subnode;
    break;

  case 36:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "E", 1);
    node->subnodes[0] = subnode;
    break;

  case 37:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "F", 1);
    node->subnodes[0] = subnode;
    break;

  case 38:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "G", 1);
    node->subnodes[0] = subnode;
    break;

  case 39:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "H", 1);
    node->subnodes[0] = subnode;
    break;

  case 40:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "I", 1);
    node->subnodes[0] = subnode;
    break;

  case 41:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "J", 1);
    node->subnodes[0] = subnode;
    break;

  case 42:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "K", 1);
    node->subnodes[0] = subnode;
    break;

  case 43:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "L", 1);
    node->subnodes[0] = subnode;
    break;

  case 44:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "M", 1);
    node->subnodes[0] = subnode;
    break;

  case 45:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "N", 1);
    node->subnodes[0] = subnode;
    break;

  case 46:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "O", 1);
    node->subnodes[0] = subnode;
    break;

  case 47:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "P", 1);
    node->subnodes[0] = subnode;
    break;

  case 48:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "Q", 1);
    node->subnodes[0] = subnode;
    break;

  case 49:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "R", 1);
    node->subnodes[0] = subnode;
    break;

  case 50:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "S", 1);
    node->subnodes[0] = subnode;
    break;

  case 51:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "T", 1);
    node->subnodes[0] = subnode;
    break;

  case 52:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "U", 1);
    node->subnodes[0] = subnode;
    break;

  case 53:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "V", 1);
    node->subnodes[0] = subnode;
    break;

  case 54:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "W", 1);
    node->subnodes[0] = subnode;
    break;

  case 55:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "X", 1);
    node->subnodes[0] = subnode;
    break;

  case 56:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "Y", 1);
    node->subnodes[0] = subnode;
    break;

  case 57:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "Z", 1);
    node->subnodes[0] = subnode;
    break;

  case 58:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "[", 1);
    node->subnodes[0] = subnode;
    break;

  case 59:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "]", 1);
    node->subnodes[0] = subnode;
    break;

  case 60:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "^", 1);
    node->subnodes[0] = subnode;
    break;

  case 61:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "_", 1);
    node->subnodes[0] = subnode;
    break;

  case 62:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "`", 1);
    node->subnodes[0] = subnode;
    break;

  case 63:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "a", 1);
    node->subnodes[0] = subnode;
    break;

  case 64:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "b", 1);
    node->subnodes[0] = subnode;
    break;

  case 65:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "c", 1);
    node->subnodes[0] = subnode;
    break;

  case 66:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "d", 1);
    node->subnodes[0] = subnode;
    break;

  case 67:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "e", 1);
    node->subnodes[0] = subnode;
    break;

  case 68:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "f", 1);
    node->subnodes[0] = subnode;
    break;

  case 69:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "g", 1);
    node->subnodes[0] = subnode;
    break;

  case 70:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "h", 1);
    node->subnodes[0] = subnode;
    break;

  case 71:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "i", 1);
    node->subnodes[0] = subnode;
    break;

  case 72:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "j", 1);
    node->subnodes[0] = subnode;
    break;

  case 73:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "k", 1);
    node->subnodes[0] = subnode;
    break;

  case 74:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "l", 1);
    node->subnodes[0] = subnode;
    break;

  case 75:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "m", 1);
    node->subnodes[0] = subnode;
    break;

  case 76:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "n", 1);
    node->subnodes[0] = subnode;
    break;

  case 77:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "o", 1);
    node->subnodes[0] = subnode;
    break;

  case 78:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "p", 1);
    node->subnodes[0] = subnode;
    break;

  case 79:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "q", 1);
    node->subnodes[0] = subnode;
    break;

  case 80:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "r", 1);
    node->subnodes[0] = subnode;
    break;

  case 81:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "s", 1);
    node->subnodes[0] = subnode;
    break;

  case 82:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "t", 1);
    node->subnodes[0] = subnode;
    break;

  case 83:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "u", 1);
    node->subnodes[0] = subnode;
    break;

  case 84:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "v", 1);
    node->subnodes[0] = subnode;
    break;

  case 85:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "w", 1);
    node->subnodes[0] = subnode;
    break;

  case 86:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "x", 1);
    node->subnodes[0] = subnode;
    break;

  case 87:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "y", 1);
    node->subnodes[0] = subnode;
    break;

  case 88:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "z", 1);
    node->subnodes[0] = subnode;
    break;

  case 89:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "{", 1);
    node->subnodes[0] = subnode;
    break;

  case 90:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "|", 1);
    node->subnodes[0] = subnode;
    break;

  case 91:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "}", 1);
    node->subnodes[0] = subnode;
    break;

  case 92:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "~", 1);
    node->subnodes[0] = subnode;
    break;

  case 93:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = gen_esc(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    break;

  }

  return node;
}

node_t *gen_esc(int depth) {
  node_t *node = node_create(ESC);

  if (depth > max_depth) {
    int val = map_rand(7);
    const char* str = pool_esc[val];
    const int str_l = pool_l_esc[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(1);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(2 * sizeof(node_t*));
    node->subnode_count = 2;
    subnode = node_create_with_val(TERM_NODE, "\\", 1);
    node->subnodes[0] = subnode;
    subnode = gen_escc(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    break;

  }

  return node;
}

node_t *gen_escc(int depth) {
  node_t *node = node_create(ESCC);

  if (depth > max_depth) {
    int val = map_rand(7);
    const char* str = pool_escc[val];
    const int str_l = pool_l_escc[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(7);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "\"", 1);
    node->subnodes[0] = subnode;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "\\", 1);
    node->subnodes[0] = subnode;
    break;

  case 2:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "b", 1);
    node->subnodes[0] = subnode;
    break;

  case 3:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "f", 1);
    node->subnodes[0] = subnode;
    break;

  case 4:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "n", 1);
    node->subnodes[0] = subnode;
    break;

  case 5:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "r", 1);
    node->subnodes[0] = subnode;
    break;

  case 6:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "t", 1);
    node->subnodes[0] = subnode;
    break;

  }

  return node;
}

node_t *gen_number(int depth) {
  node_t *node = node_create(NUMBER);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_number[val];
    const int str_l = pool_l_number[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(1);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(3 * sizeof(node_t*));
    node->subnode_count = 3;
    subnode = gen_int(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    subnode = gen_frac(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    subnode = gen_exp(depth +1);
    node->non_term_size += 1;
    node->subnodes[2] = subnode;
    break;

  }

  return node;
}

node_t *gen_int(int depth) {
  node_t *node = node_create(INT);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_int[val];
    const int str_l = pool_l_int[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(4);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = gen_digit(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(2 * sizeof(node_t*));
    node->subnode_count = 2;
    subnode = node_create_with_val(TERM_NODE, "-", 1);
    node->subnodes[0] = subnode;
    subnode = gen_digits(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    break;

  case 2:
    node->subnodes = (node_t**)malloc(3 * sizeof(node_t*));
    node->subnode_count = 3;
    subnode = node_create_with_val(TERM_NODE, "-", 1);
    node->subnodes[0] = subnode;
    subnode = gen_onenine(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    subnode = gen_digits(depth +1);
    node->non_term_size += 1;
    node->subnodes[2] = subnode;
    break;

  case 3:
    node->subnodes = (node_t**)malloc(2 * sizeof(node_t*));
    node->subnode_count = 2;
    subnode = gen_onenine(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    subnode = gen_digits(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    break;

  }

  return node;
}

node_t *gen_digits(int depth) {
  node_t *node = node_create(DIGITS);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_digits[val];
    const int str_l = pool_l_digits[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(1);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = gen_digit_1(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    break;

  }

  return node;
}

node_t *gen_digit(int depth) {
  node_t *node = node_create(DIGIT);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_digit[val];
    const int str_l = pool_l_digit[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(2);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "0", 1);
    node->subnodes[0] = subnode;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = gen_onenine(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    break;

  }

  return node;
}

node_t *gen_onenine(int depth) {
  node_t *node = node_create(ONENINE);

  if (depth > max_depth) {
    int val = map_rand(9);
    const char* str = pool_onenine[val];
    const int str_l = pool_l_onenine[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(9);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "1", 1);
    node->subnodes[0] = subnode;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "2", 1);
    node->subnodes[0] = subnode;
    break;

  case 2:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "3", 1);
    node->subnodes[0] = subnode;
    break;

  case 3:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "4", 1);
    node->subnodes[0] = subnode;
    break;

  case 4:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "5", 1);
    node->subnodes[0] = subnode;
    break;

  case 5:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "6", 1);
    node->subnodes[0] = subnode;
    break;

  case 6:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "7", 1);
    node->subnodes[0] = subnode;
    break;

  case 7:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "8", 1);
    node->subnodes[0] = subnode;
    break;

  case 8:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "9", 1);
    node->subnodes[0] = subnode;
    break;

  }

  return node;
}

node_t *gen_frac(int depth) {
  node_t *node = node_create(FRAC);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_frac[val];
    const int str_l = pool_l_frac[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(2);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(0 * sizeof(node_t*));
    node->subnode_count = 0;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(2 * sizeof(node_t*));
    node->subnode_count = 2;
    subnode = node_create_with_val(TERM_NODE, ".", 1);
    node->subnodes[0] = subnode;
    subnode = gen_digits(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    break;

  }

  return node;
}

node_t *gen_exp(int depth) {
  node_t *node = node_create(EXP);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_exp[val];
    const int str_l = pool_l_exp[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(3);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(0 * sizeof(node_t*));
    node->subnode_count = 0;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(3 * sizeof(node_t*));
    node->subnode_count = 3;
    subnode = node_create_with_val(TERM_NODE, "E", 1);
    node->subnodes[0] = subnode;
    subnode = gen_sign(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    subnode = gen_digits(depth +1);
    node->non_term_size += 1;
    node->subnodes[2] = subnode;
    break;

  case 2:
    node->subnodes = (node_t**)malloc(3 * sizeof(node_t*));
    node->subnode_count = 3;
    subnode = node_create_with_val(TERM_NODE, "e", 1);
    node->subnodes[0] = subnode;
    subnode = gen_sign(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    subnode = gen_digits(depth +1);
    node->non_term_size += 1;
    node->subnodes[2] = subnode;
    break;

  }

  return node;
}

node_t *gen_sign(int depth) {
  node_t *node = node_create(SIGN);

  if (depth > max_depth) {
    int val = map_rand(3);
    const char* str = pool_sign[val];
    const int str_l = pool_l_sign[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(3);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(0 * sizeof(node_t*));
    node->subnode_count = 0;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "+", 1);
    node->subnodes[0] = subnode;
    break;

  case 2:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "-", 1);
    node->subnodes[0] = subnode;
    break;

  }

  return node;
}

node_t *gen_ws(int depth) {
  node_t *node = node_create(WS);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_ws[val];
    const int str_l = pool_l_ws[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(2);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(0 * sizeof(node_t*));
    node->subnode_count = 0;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(2 * sizeof(node_t*));
    node->subnode_count = 2;
    subnode = gen_sp1(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    subnode = gen_ws(depth +1);
    node->non_term_size += 1;
    node->recursion_edge_size += 1;
    node->subnodes[1] = subnode;
    break;

  }

  return node;
}

node_t *gen_sp1(int depth) {
  node_t *node = node_create(SP1);

  if (depth > max_depth) {
    int val = map_rand(4);
    const char* str = pool_sp1[val];
    const int str_l = pool_l_sp1[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(4);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "\t", 1);
    node->subnodes[0] = subnode;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "\n", 1);
    node->subnodes[0] = subnode;
    break;

  case 2:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, "\r", 1);
    node->subnodes[0] = subnode;
    break;

  case 3:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = node_create_with_val(TERM_NODE, " ", 1);
    node->subnodes[0] = subnode;
    break;

  }

  return node;
}

node_t *gen_symbol(int depth) {
  node_t *node = node_create(SYMBOL);

  if (depth > max_depth) {
    int val = map_rand(3);
    const char* str = pool_symbol[val];
    const int str_l = pool_l_symbol[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(1);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(2 * sizeof(node_t*));
    node->subnode_count = 2;
    subnode = node_create_with_val(TERM_NODE, ",", 1);
    node->subnodes[0] = subnode;
    subnode = gen_members(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    break;

  }

  return node;
}

node_t *gen_symbol_1(int depth) {
  node_t *node = node_create(SYMBOL_1);

  if (depth > max_depth) {
    int val = map_rand(3);
    const char* str = pool_symbol_1[val];
    const int str_l = pool_l_symbol_1[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(1);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(2 * sizeof(node_t*));
    node->subnode_count = 2;
    subnode = node_create_with_val(TERM_NODE, ",", 1);
    node->subnodes[0] = subnode;
    subnode = gen_elements(depth +1);
    node->non_term_size += 1;
    node->subnodes[1] = subnode;
    break;

  }

  return node;
}

node_t *gen_symbol_2(int depth) {
  node_t *node = node_create(SYMBOL_2);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_symbol_2[val];
    const int str_l = pool_l_symbol_2[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(2);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(0 * sizeof(node_t*));
    node->subnode_count = 0;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(2 * sizeof(node_t*));
    node->subnode_count = 2;
    subnode = gen_symbol(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    subnode = gen_symbol_2(depth +1);
    node->non_term_size += 1;
    node->recursion_edge_size += 1;
    node->subnodes[1] = subnode;
    break;

  }

  return node;
}

node_t *gen_symbol_1_1(int depth) {
  node_t *node = node_create(SYMBOL_1_1);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_symbol_1_1[val];
    const int str_l = pool_l_symbol_1_1[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(2);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(0 * sizeof(node_t*));
    node->subnode_count = 0;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(2 * sizeof(node_t*));
    node->subnode_count = 2;
    subnode = gen_symbol_1(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    subnode = gen_symbol_1_1(depth +1);
    node->non_term_size += 1;
    node->recursion_edge_size += 1;
    node->subnodes[1] = subnode;
    break;

  }

  return node;
}

node_t *gen_character_1(int depth) {
  node_t *node = node_create(CHARACTER_1);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_character_1[val];
    const int str_l = pool_l_character_1[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(2);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(0 * sizeof(node_t*));
    node->subnode_count = 0;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(2 * sizeof(node_t*));
    node->subnode_count = 2;
    subnode = gen_character(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    subnode = gen_character_1(depth +1);
    node->non_term_size += 1;
    node->recursion_edge_size += 1;
    node->subnodes[1] = subnode;
    break;

  }

  return node;
}

node_t *gen_digit_1(int depth) {
  node_t *node = node_create(DIGIT_1);

  if (depth > max_depth) {
    int val = map_rand(1);
    const char* str = pool_digit_1[val];
    const int str_l = pool_l_digit_1[val];
    node_set_val(node, str, str_l);
    return node;
  }

  int val = map_rand(2);
  node_t *subnode = NULL;
  switch(val) {

  case 0:
    node->subnodes = (node_t**)malloc(1 * sizeof(node_t*));
    node->subnode_count = 1;
    subnode = gen_digit(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    break;

  case 1:
    node->subnodes = (node_t**)malloc(2 * sizeof(node_t*));
    node->subnode_count = 2;
    subnode = gen_digit(depth +1);
    node->non_term_size += 1;
    node->subnodes[0] = subnode;
    subnode = gen_digit_1(depth +1);
    node->non_term_size += 1;
    node->recursion_edge_size += 1;
    node->subnodes[1] = subnode;
    break;

  }

  return node;
}


gen_func_t gen_funcs[31] = {
  NULL,
  gen_start,
  gen_json,
  gen_element,
  gen_value,
  gen_object,
  gen_members,
  gen_member,
  gen_array,
  gen_elements,
  gen_string,
  gen_characters,
  gen_character,
  gen_esc,
  gen_escc,
  gen_number,
  gen_int,
  gen_digits,
  gen_digit,
  gen_onenine,
  gen_frac,
  gen_exp,
  gen_sign,
  gen_ws,
  gen_sp1,
  gen_symbol,
  gen_symbol_1,
  gen_symbol_2,
  gen_symbol_1_1,
  gen_character_1,
  gen_digit_1,
};


tree_t *gen_init__() {
  tree_t *tree = tree_create();
  tree->root = gen_start(0);
  return tree;
}
