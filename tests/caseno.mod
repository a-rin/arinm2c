MODULE caseno;
VAR i: INTEGER;
BEGIN
  i := 5;
  CASE i OF
    1: WRITELN(1)
  | 2: WRITELN(2)
  END
END caseno.
