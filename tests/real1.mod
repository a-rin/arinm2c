MODULE real1;
CONST Half = 0.5;
VAR x: REAL; y: LONGREAL; i: INTEGER;
BEGIN
  x := 1.5;
  x := x * 2.0 + Half;
  y := 2.25;
  y := y / 2.0;
  i := TRUNC(x);
  x := FLOAT(i) / 4.0;
  y := VAL(LONGREAL, x) + y;
  IF (x < 1.0) & (y > 1.5) THEN WRITELN(i, ' ', x, ' ', y) END;
  y := 1.5E3 + 0.25;
  WRITELN(y);
  x := -x;
  WRITELN(ABS(x), ' ', x)
END real1.
