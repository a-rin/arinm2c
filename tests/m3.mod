MODULE m3;
VAR x, X, Foo: LONGINT;
BEGIN
  x := 1;
  X := 2;
  Foo := x + X * 10;
  IF (x # X) & ~(Foo = 0) THEN
    WRITELN(Foo)
  ELSE
    WRITELN(0)
  END;
  IF x # 1 THEN WRITELN(901) ELSE WRITELN(902) END
END m3.
