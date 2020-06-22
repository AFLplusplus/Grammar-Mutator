
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>


void out(const char s);

int map(int v);

extern int max_depth;
void gen_start(int depth);
void gen_json(int depth);
void gen_element(int depth);
void gen_value(int depth);
void gen_object(int depth);
void gen_members(int depth);
void gen_member(int depth);
void gen_array(int depth);
void gen_elements(int depth);
void gen_string(int depth);
void gen_characters(int depth);
void gen_character(int depth);
void gen_esc(int depth);
void gen_escc(int depth);
void gen_number(int depth);
void gen_int(int depth);
void gen_digits(int depth);
void gen_digit(int depth);
void gen_onenine(int depth);
void gen_frac(int depth);
void gen_exp(int depth);
void gen_sign(int depth);
void gen_ws(int depth);
void gen_sp1(int depth);
void gen_symbol(int depth);
void gen_symbol_1(int depth);
void gen_symbol_2(int depth);
void gen_symbol_1_1(int depth);
void gen_character_1(int depth);
void gen_digit_1(int depth);

const char* pool_start[] =  {"null", "true", "false"};
const int pool_l_start[] =  {4, 4, 5};


const char* pool_json[] =  {"false", "true", "null"};
const int pool_l_json[] =  {5, 4, 4};


const char* pool_element[] =  {"true", "false", "null"};
const int pool_l_element[] =  {4, 5, 4};


const char* pool_value[] =  {"true", "false", "null"};
const int pool_l_value[] =  {4, 5, 4};


const char* pool_object[] =  {"{}"};
const int pool_l_object[] =  {2};


const char* pool_members[] =  {"\"\":true", "\"\":null", "\"\":false"};
const int pool_l_members[] =  {7, 7, 8};


const char* pool_member[] =  {"\"\":null", "\"\":false", "\"\":true"};
const int pool_l_member[] =  {7, 8, 7};


const char* pool_array[] =  {"[]"};
const int pool_l_array[] =  {2};


const char* pool_elements[] =  {"false", "null", "true"};
const int pool_l_elements[] =  {5, 4, 4};


const char* pool_string[] =  {"\"\""};
const int pool_l_string[] =  {2};


const char* pool_characters[] =  {""};
const int pool_l_characters[] =  {0};


const char* pool_character[] =  {"v", "d", "H", "r", "w", "4", "F", ",", ":", "8", "N", "a", "x", "[", "]", "k", "f", "l", "2", "i", "|", "K", "b", "#", "*", "~", "y", "`", "Y", "Q", "<", "e", "+", "j", "&", "J", "C", "M", "s", "S", "$", ".", "=", "/", "\"", ")", "L", "P", "^", "u", "@", "5", "I", "G", "h", "}", "n", "9", "o", "U", "W", " ", "Z", "g", "p", "E", "q", "T", "7", "0", "?", "D", "-", "%", "!", "A", "O", "6", "_", "c", "t", "B", "3", "V", "{", ";", ">", "X", "1", "R", "(", "m", "z"};
const int pool_l_character[] =  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};


const char* pool_esc[] =  {"\\\\", "\\\"", "\\r", "\\n", "\\t", "\\f", "\\b"};
const int pool_l_esc[] =  {2, 2, 2, 2, 2, 2, 2};


const char* pool_escc[] =  {"n", "\"", "\\", "t", "b", "r", "f"};
const int pool_l_escc[] =  {1, 1, 1, 1, 1, 1, 1};


const char* pool_number[] =  {"0"};
const int pool_l_number[] =  {1};


const char* pool_int[] =  {"0"};
const int pool_l_int[] =  {1};


const char* pool_digits[] =  {"0"};
const int pool_l_digits[] =  {1};


const char* pool_digit[] =  {"0"};
const int pool_l_digit[] =  {1};


const char* pool_onenine[] =  {"9", "8", "3", "7", "5", "1", "2", "6", "4"};
const int pool_l_onenine[] =  {1, 1, 1, 1, 1, 1, 1, 1, 1};


const char* pool_frac[] =  {""};
const int pool_l_frac[] =  {0};


const char* pool_exp[] =  {""};
const int pool_l_exp[] =  {0};


const char* pool_sign[] =  {"-", "", "+"};
const int pool_l_sign[] =  {1, 0, 1};


const char* pool_ws[] =  {""};
const int pool_l_ws[] =  {0};


const char* pool_sp1[] =  {"\t", "\r", "\n", " "};
const int pool_l_sp1[] =  {1, 1, 1, 1};


const char* pool_symbol[] =  {",\"\":null", ",\"\":false", ",\"\":true"};
const int pool_l_symbol[] =  {8, 9, 8};


const char* pool_symbol_1[] =  {",null", ",false", ",true"};
const int pool_l_symbol_1[] =  {5, 6, 5};


const char* pool_symbol_2[] =  {""};
const int pool_l_symbol_2[] =  {0};


const char* pool_symbol_1_1[] =  {""};
const int pool_l_symbol_1_1[] =  {0};


const char* pool_character_1[] =  {""};
const int pool_l_character_1[] =  {0};


const char* pool_digit_1[] =  {"0"};
const int pool_l_digit_1[] =  {1};


void gen_start(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_start[val];
        const int str_l = pool_l_start[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_json(depth +1);
        break;

    }
}


void gen_json(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_json[val];
        const int str_l = pool_l_json[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_element(depth +1);
        break;

    }
}


void gen_element(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_element[val];
        const int str_l = pool_l_element[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_ws(depth +1);
        gen_value(depth +1);
        gen_ws(depth +1);
        break;

    }
}


void gen_value(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_value[val];
        const int str_l = pool_l_value[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(7);
    switch(val) {

    case 0:
        out('f');
        out('a');
        out('l');
        out('s');
        out('e');
        break;

    case 1:
        out('n');
        out('u');
        out('l');
        out('l');
        break;

    case 2:
        out('t');
        out('r');
        out('u');
        out('e');
        break;

    case 3:
        gen_array(depth +1);
        break;

    case 4:
        gen_object(depth +1);
        break;

    case 5:
        gen_number(depth +1);
        break;

    case 6:
        gen_string(depth +1);
        break;

    }
}


void gen_object(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_object[val];
        const int str_l = pool_l_object[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:
        out('{');
        gen_ws(depth +1);
        out('}');
        break;

    case 1:
        out('{');
        gen_members(depth +1);
        out('}');
        break;

    }
}


void gen_members(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_members[val];
        const int str_l = pool_l_members[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_member(depth +1);
        gen_symbol_2(depth +1);
        break;

    }
}


void gen_member(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_member[val];
        const int str_l = pool_l_member[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_ws(depth +1);
        gen_string(depth +1);
        gen_ws(depth +1);
        out(':');
        gen_element(depth +1);
        break;

    }
}


void gen_array(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_array[val];
        const int str_l = pool_l_array[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:
        out('[');
        gen_ws(depth +1);
        out(']');
        break;

    case 1:
        out('[');
        gen_elements(depth +1);
        out(']');
        break;

    }
}


void gen_elements(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_elements[val];
        const int str_l = pool_l_elements[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_element(depth +1);
        gen_symbol_1_1(depth +1);
        break;

    }
}


void gen_string(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_string[val];
        const int str_l = pool_l_string[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        out('"');
        gen_characters(depth +1);
        out('"');
        break;

    }
}


void gen_characters(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_characters[val];
        const int str_l = pool_l_characters[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_character_1(depth +1);
        break;

    }
}


void gen_character(int depth) {
    if (depth > max_depth) {
        int val = map(93);
        const char* str = pool_character[val];
        const int str_l = pool_l_character[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(94);
    switch(val) {

    case 0:
        out(' ');
        break;

    case 1:
        out('!');
        break;

    case 2:
        out('"');
        break;

    case 3:
        out('#');
        break;

    case 4:
        out('$');
        break;

    case 5:
        out('%');
        break;

    case 6:
        out('&');
        break;

    case 7:
        out('(');
        break;

    case 8:
        out(')');
        break;

    case 9:
        out('*');
        break;

    case 10:
        out('+');
        break;

    case 11:
        out(',');
        break;

    case 12:
        out('-');
        break;

    case 13:
        out('.');
        break;

    case 14:
        out('/');
        break;

    case 15:
        out('0');
        break;

    case 16:
        out('1');
        break;

    case 17:
        out('2');
        break;

    case 18:
        out('3');
        break;

    case 19:
        out('4');
        break;

    case 20:
        out('5');
        break;

    case 21:
        out('6');
        break;

    case 22:
        out('7');
        break;

    case 23:
        out('8');
        break;

    case 24:
        out('9');
        break;

    case 25:
        out(':');
        break;

    case 26:
        out(';');
        break;

    case 27:
        out('<');
        break;

    case 28:
        out('=');
        break;

    case 29:
        out('>');
        break;

    case 30:
        out('?');
        break;

    case 31:
        out('@');
        break;

    case 32:
        out('A');
        break;

    case 33:
        out('B');
        break;

    case 34:
        out('C');
        break;

    case 35:
        out('D');
        break;

    case 36:
        out('E');
        break;

    case 37:
        out('F');
        break;

    case 38:
        out('G');
        break;

    case 39:
        out('H');
        break;

    case 40:
        out('I');
        break;

    case 41:
        out('J');
        break;

    case 42:
        out('K');
        break;

    case 43:
        out('L');
        break;

    case 44:
        out('M');
        break;

    case 45:
        out('N');
        break;

    case 46:
        out('O');
        break;

    case 47:
        out('P');
        break;

    case 48:
        out('Q');
        break;

    case 49:
        out('R');
        break;

    case 50:
        out('S');
        break;

    case 51:
        out('T');
        break;

    case 52:
        out('U');
        break;

    case 53:
        out('V');
        break;

    case 54:
        out('W');
        break;

    case 55:
        out('X');
        break;

    case 56:
        out('Y');
        break;

    case 57:
        out('Z');
        break;

    case 58:
        out('[');
        break;

    case 59:
        out(']');
        break;

    case 60:
        out('^');
        break;

    case 61:
        out('_');
        break;

    case 62:
        out('`');
        break;

    case 63:
        out('a');
        break;

    case 64:
        out('b');
        break;

    case 65:
        out('c');
        break;

    case 66:
        out('d');
        break;

    case 67:
        out('e');
        break;

    case 68:
        out('f');
        break;

    case 69:
        out('g');
        break;

    case 70:
        out('h');
        break;

    case 71:
        out('i');
        break;

    case 72:
        out('j');
        break;

    case 73:
        out('k');
        break;

    case 74:
        out('l');
        break;

    case 75:
        out('m');
        break;

    case 76:
        out('n');
        break;

    case 77:
        out('o');
        break;

    case 78:
        out('p');
        break;

    case 79:
        out('q');
        break;

    case 80:
        out('r');
        break;

    case 81:
        out('s');
        break;

    case 82:
        out('t');
        break;

    case 83:
        out('u');
        break;

    case 84:
        out('v');
        break;

    case 85:
        out('w');
        break;

    case 86:
        out('x');
        break;

    case 87:
        out('y');
        break;

    case 88:
        out('z');
        break;

    case 89:
        out('{');
        break;

    case 90:
        out('|');
        break;

    case 91:
        out('}');
        break;

    case 92:
        out('~');
        break;

    case 93:
        gen_esc(depth +1);
        break;

    }
}


void gen_esc(int depth) {
    if (depth > max_depth) {
        int val = map(7);
        const char* str = pool_esc[val];
        const int str_l = pool_l_esc[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        out('\\');
        gen_escc(depth +1);
        break;

    }
}


void gen_escc(int depth) {
    if (depth > max_depth) {
        int val = map(7);
        const char* str = pool_escc[val];
        const int str_l = pool_l_escc[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(7);
    switch(val) {

    case 0:
        out('"');
        break;

    case 1:
        out('\\');
        break;

    case 2:
        out('b');
        break;

    case 3:
        out('f');
        break;

    case 4:
        out('n');
        break;

    case 5:
        out('r');
        break;

    case 6:
        out('t');
        break;

    }
}


void gen_number(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_number[val];
        const int str_l = pool_l_number[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_int(depth +1);
        gen_frac(depth +1);
        gen_exp(depth +1);
        break;

    }
}


void gen_int(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_int[val];
        const int str_l = pool_l_int[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(4);
    switch(val) {

    case 0:
        gen_digit(depth +1);
        break;

    case 1:
        out('-');
        gen_digits(depth +1);
        break;

    case 2:
        out('-');
        gen_onenine(depth +1);
        gen_digits(depth +1);
        break;

    case 3:
        gen_onenine(depth +1);
        gen_digits(depth +1);
        break;

    }
}


void gen_digits(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_digits[val];
        const int str_l = pool_l_digits[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        gen_digit_1(depth +1);
        break;

    }
}


void gen_digit(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_digit[val];
        const int str_l = pool_l_digit[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:
        out('0');
        break;

    case 1:
        gen_onenine(depth +1);
        break;

    }
}


void gen_onenine(int depth) {
    if (depth > max_depth) {
        int val = map(9);
        const char* str = pool_onenine[val];
        const int str_l = pool_l_onenine[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(9);
    switch(val) {

    case 0:
        out('1');
        break;

    case 1:
        out('2');
        break;

    case 2:
        out('3');
        break;

    case 3:
        out('4');
        break;

    case 4:
        out('5');
        break;

    case 5:
        out('6');
        break;

    case 6:
        out('7');
        break;

    case 7:
        out('8');
        break;

    case 8:
        out('9');
        break;

    }
}


void gen_frac(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_frac[val];
        const int str_l = pool_l_frac[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:

        break;

    case 1:
        out('.');
        gen_digits(depth +1);
        break;

    }
}


void gen_exp(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_exp[val];
        const int str_l = pool_l_exp[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(3);
    switch(val) {

    case 0:

        break;

    case 1:
        out('E');
        gen_sign(depth +1);
        gen_digits(depth +1);
        break;

    case 2:
        out('e');
        gen_sign(depth +1);
        gen_digits(depth +1);
        break;

    }
}


void gen_sign(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_sign[val];
        const int str_l = pool_l_sign[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(3);
    switch(val) {

    case 0:

        break;

    case 1:
        out('+');
        break;

    case 2:
        out('-');
        break;

    }
}


void gen_ws(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_ws[val];
        const int str_l = pool_l_ws[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:

        break;

    case 1:
        gen_sp1(depth +1);
        gen_ws(depth +1);
        break;

    }
}


void gen_sp1(int depth) {
    if (depth > max_depth) {
        int val = map(4);
        const char* str = pool_sp1[val];
        const int str_l = pool_l_sp1[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(4);
    switch(val) {

    case 0:
        out('\t');
        break;

    case 1:
        out('\n');
        break;

    case 2:
        out('\r');
        break;

    case 3:
        out(' ');
        break;

    }
}


void gen_symbol(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_symbol[val];
        const int str_l = pool_l_symbol[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        out(',');
        gen_members(depth +1);
        break;

    }
}


void gen_symbol_1(int depth) {
    if (depth > max_depth) {
        int val = map(3);
        const char* str = pool_symbol_1[val];
        const int str_l = pool_l_symbol_1[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(1);
    switch(val) {

    case 0:
        out(',');
        gen_elements(depth +1);
        break;

    }
}


void gen_symbol_2(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_symbol_2[val];
        const int str_l = pool_l_symbol_2[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:

        break;

    case 1:
        gen_symbol(depth +1);
        gen_symbol_2(depth +1);
        break;

    }
}


void gen_symbol_1_1(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_symbol_1_1[val];
        const int str_l = pool_l_symbol_1_1[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:

        break;

    case 1:
        gen_symbol_1(depth +1);
        gen_symbol_1_1(depth +1);
        break;

    }
}


void gen_character_1(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_character_1[val];
        const int str_l = pool_l_character_1[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:

        break;

    case 1:
        gen_character(depth +1);
        gen_character_1(depth +1);
        break;

    }
}


void gen_digit_1(int depth) {
    if (depth > max_depth) {
        int val = map(1);
        const char* str = pool_digit_1[val];
        const int str_l = pool_l_digit_1[val];
        for (int i = 0; i < str_l; i++) {
            out(str[i]);
        }
        return;
    }

    int val = map(2);
    switch(val) {

    case 0:
        gen_digit(depth +1);
        break;

    case 1:
        gen_digit(depth +1);
        gen_digit_1(depth +1);
        break;

    }
}


void gen_init__() {
    gen_start(0);
    out('\n');
    return;
}
