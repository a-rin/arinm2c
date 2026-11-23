MODULE divneg;
VAR a, b: LONGINT;
BEGIN
  a := -7; b := 3;
  WRITELN(a DIV b, " ", a MOD b);
  WRITELN(-7 DIV 3, " ", -7 MOD 3);
  WRITELN(7 DIV 3, " ", 7 MOD 3)
END divneg.
