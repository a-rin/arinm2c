MODULE nest3;
VAR a: ARRAY [0..3] OF LONGINT;
VAR r: LONGINT;

PROCEDURE Sum(v: ARRAY OF LONGINT): LONGINT;
VAR s: LONGINT;

  PROCEDURE Walk(i: LONGINT);
  BEGIN
    IF i <= HIGH(v) THEN
      s := s + v[i];
      Walk(i + 1)
    END
  END Walk;

BEGIN
  s := 0;
  Walk(0);
  RETURN s
END Sum;

BEGIN
  a[0] := 1; a[1] := 2; a[2] := 3; a[3] := 4;
  r := Sum(a);
  WRITELN(r)
END nest3.
