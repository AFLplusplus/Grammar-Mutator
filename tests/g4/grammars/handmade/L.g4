lexer grammar L;
options { superClass = TokenBase; }
@header {#include "TokenBase.h"}
ID : [a-z]+ ;
WS : ' ' -> skip ;
