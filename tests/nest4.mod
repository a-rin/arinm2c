MODULE nest4;

PROCEDURE P1(): LONGINT;
TYPE Rec = RECORD a: LONGINT END;
VAR r: Rec;
BEGIN
  r.a := 4;
  RETURN r.a
END P1;

PROCEDURE P2(): LONGINT;
TYPE Rec = RECORD x, y: LONGINT END;
VAR r: Rec;

  PROCEDURE Sq(v: LONGINT): LONGINT;
  BEGIN
    RETURN v * v
  END Sq;

BEGIN
  r.x := 3;
  r.y := Sq(r.x);
  RETURN r.x + r.y
END P2;

BEGIN
  WRITELN(P1(), ' ', P2())
END nest4.
