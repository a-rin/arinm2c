MODULE idx3;
TYPE Color = (red, green, blue);
VAR n: ARRAY Color OF INTEGER; c: Color; g: ARRAY [0..1], [0..2] OF INTEGER; i, j: INTEGER;
BEGIN
  FOR c := red TO blue DO n[c] := ORD(c) END;
  i := 1; j := 2;
  g[i][j] := 7;
  WRITELN(n[blue], " ", g[1][2]);
  j := 3;
  g[i][j] := 1
END idx3.
