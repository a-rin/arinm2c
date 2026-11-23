MODULE real2;
VAR s: SHORTREAL; r: REAL; l: LONGREAL;
BEGIN
  s := 0.5; r := 0.1; l := 0.1;
  r := r + VAL(REAL, s);
  l := VAL(LONGREAL, r) + l;
  WRITELN(s, ' ', r, ' ', l)
END real2.
