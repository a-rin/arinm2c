MODULE s2;
(* VAR parameters and open arrays *)
VAR a, b: LONGINT; v: ARRAY [0..3] OF LONGINT; t: LONGINT;

PROCEDURE Swap(VAR x, y: LONGINT);
VAR tmp: LONGINT;
BEGIN
  tmp := x; x := y; y := tmp
END Swap;

PROCEDURE Sum(a: ARRAY OF LONGINT): LONGINT;
VAR i, s: LONGINT;
BEGIN
  s := 0;
  FOR i := 0 TO HIGH(a) DO s := s + a[i] END;
  RETURN s
END Sum;

BEGIN
  a := 1; b := 2;
  Swap(a, b);
  v[0] := 1; v[1] := 2; v[2] := 3; v[3] := 4;
  t := Sum(v);
  WRITELN(a, ' ', b, ' ', t)
END s2.
