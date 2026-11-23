MODULE B2;
FROM SYSTEM IMPORT BYTE;
TYPE Pt = RECORD x, y: INTEGER END;
VAR i, j: INTEGER; k: LONGINT; p, q: Pt; s: ARRAY [0..3] OF CHAR;

PROCEDURE Move(VAR src, dst: ARRAY OF BYTE);
VAR n: INTEGER;
BEGIN
  FOR n := 0 TO HIGH(src) DO
    dst[n] := src[n]
  END
END Move;

BEGIN
  i := 305419896;
  Move(i, j);
  k := 0;
  Move(i, k);
  p.x := 3; p.y := 4;
  Move(p, q);
  s[0] := 'a'; s[1] := 'b'; s[2] := 'c'; s[3] := 'd';
  Move(s, i);
  WRITELN(j, ' ', k, ' ', q.x, ' ', q.y, ' ', i)
END B2.
