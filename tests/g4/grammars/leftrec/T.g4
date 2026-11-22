grammar T;
s : e EOF ;
e : e '+' e | INT ;
INT : [0-9]+ ;
WS : ' ' -> skip ;
