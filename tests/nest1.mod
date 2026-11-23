MODULE nest1;
VAR total: LONGINT;

PROCEDURE Outer(n: LONGINT): LONGINT;
VAR acc: LONGINT;

  PROCEDURE Add(k: LONGINT);
  BEGIN
    acc := acc + k + n
  END Add;

  PROCEDURE Twice(k: LONGINT);
  BEGIN
    Add(k);
    Add(k)
  END Twice;

BEGIN
  acc := 0;
  Twice(1);
  Add(10);
  RETURN acc
END Outer;

BEGIN
  total := Outer(5);
  WRITELN(total)
END nest1.
