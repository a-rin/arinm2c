MODULE Val;
VAR a: INT32; b: INT64;
BEGIN
  a := 5; b := 5;
  b := b + 1; a := a + 1;
  b := VAL(INT64, a) + b;
  a := VAL(INT32, b) - 3;
  WRITELN(a, " ", b)
END Val.
