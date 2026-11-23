MODULE signs;
VAR i: INTEGER; c: CARDINAL;
BEGIN
  i := 5; c := 7;
  c := i;
  i := c;
  WRITELN(i + VAL(INTEGER, c))
END signs.
