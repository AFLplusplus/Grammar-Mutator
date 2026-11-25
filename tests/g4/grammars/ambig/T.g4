grammar T;
s : '[' elements ']' EOF ;
elements : item more ;
more : | next more ;
next : ',' elements ;
item : '0' ;
WS : ' ' -> skip ;
