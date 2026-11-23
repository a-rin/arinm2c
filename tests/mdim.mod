MODULE mdim;
TYPE Grid = ARRAY [0..2], [0..3] OF LONGINT;
VAR g: Grid;

PROCEDURE Sum(m: ARRAY OF ARRAY OF LONGINT): LONGINT;
VAR i, j, s: LONGINT;
BEGIN
  s := 0;
  FOR i := 0 TO HIGH(m) DO
    FOR j := 0 TO HIGH(m[0]) DO s := s + m[i][j] END
  END;
  RETURN s
END Sum;

PROCEDURE Fill(VAR m: ARRAY OF ARRAY OF LONGINT);
VAR i, j: LONGINT;
BEGIN
  FOR i := 0 TO HIGH(m) DO
    FOR j := 0 TO HIGH(m[0]) DO m[i][j] := i * 10 + j END
  END
END Fill;

BEGIN
  Fill(g);
  WRITELN(Sum(g), ' ', HIGH(g), ' ', g[2][3])
END mdim.
