MODULE m6;
VAR file, label, unit, string, type: LONGINT;
BEGIN
  file := 1; label := 2; unit := 3; string := 4; type := 5;
  WRITELN(file + label + unit + string + type);
  WRITELN(SIZEOF(INTEGER), ' ', SIZEOF(CARDINAL), ' ', SIZEOF(LONGINT), ' ', SIZEOF(BOOLEAN), ' ', SIZEOF(CHAR))
END m6.
