MODULE m2;
VAR i: LONGINT;
BEGIN
  i := 0;
  WHILE i < 4 DO
    IF i = 0 THEN
      WRITELN(100)
    ELSIF i = 1 THEN
      WRITELN(101)
    ELSIF i = 2 THEN
      WRITELN(102)
    ELSE
      WRITELN(199)
    END;
    i := i + 1
  END
END m2.
