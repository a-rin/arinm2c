MODULE idx1;
VAR a: ARRAY [1..4] OF INTEGER; i: INTEGER;
BEGIN
  i := 5;
  a[1] := 1;
  a[i] := 9;
  WRITELN(a[1])
END idx1.
