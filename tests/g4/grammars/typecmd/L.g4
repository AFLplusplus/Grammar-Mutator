lexer grammar L;
tokens { Y, Z }
A : 'a' -> type(Y) ;
B : 'b' -> type(Z) ;
C : 'c' -> type(Z) ;
WS : ' ' -> skip ;
