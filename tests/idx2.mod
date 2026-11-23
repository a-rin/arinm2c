MODULE idx2;
VAR v: ARRAY [0..2] OF LONGINT;

PROCEDURE Get(a: ARRAY OF LONGINT; i: LONGINT): LONGINT;
BEGIN
  RETURN a[i]
END Get;

BEGIN
  v[0] := 1; v[1] := 2; v[2] := 3;
  WRITELN(Get(v, 2));
  WRITELN(Get(v, 3))
END idx2.
