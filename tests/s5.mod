MODULE S5;
(* WITH and variant records *)
TYPE
  Pt = RECORD x, y: LONGINT END;
  Shape = RECORD
    CASE isRound: BOOLEAN OF
      TRUE: radius: LONGINT
    | FALSE: w, h: LONGINT
    END
  END;
VAR p: Pt; s: Shape;
BEGIN
  WITH p DO
    x := 3;
    y := 4
  END;
  s.isRound := FALSE;
  s.w := 5; s.h := 6;
  WRITELN(p.x * p.y, ' ', s.w + s.h)
END S5.
