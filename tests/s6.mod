MODULE s6;
(* ARRAY index forms and the ELSE branch of a variant record *)
TYPE
  Color = (red, green, blue);
  Count = ARRAY Color OF INTEGER;
  Grid = ARRAY [0..2], [0..3] OF INTEGER;
  Cell = RECORD
    CASE kind: INTEGER OF
      1: i: INTEGER
    | 2: x, y: INTEGER
    ELSE z: INTEGER
    END
  END;
VAR n: Count; g: Grid; r, c: INTEGER; cl: Cell; s: INTEGER;
BEGIN
  n[red] := 1; n[green] := 20; n[blue] := 300;
  FOR r := 0 TO 2 DO
    FOR c := 0 TO 3 DO
      g[r][c] := r * 10 + c
    END
  END;
  cl.kind := 7;
  cl.z := 5;
  s := n[red] + n[green] + n[blue] + g[2][3] + cl.z;
  WRITELN(s)
END s6.
