MODULE m7bad3;
VAR a: INT32; b: INT64;
BEGIN
  a := 1; b := 1;
  a := VAL(INT32, b + a)
END m7bad3.
