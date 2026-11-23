MODULE nest2;
VAR v: LONGINT;

PROCEDURE A(VAR r: LONGINT; x: LONGINT);
TYPE Pair = RECORD a, b: LONGINT END;
VAR p: Pair;

  PROCEDURE B(y: LONGINT);

    PROCEDURE C(z: LONGINT);
    BEGIN
      IF z > 0 THEN
        r := r + x + y + p.a;
        C(z - 1)
      END
    END C;

  BEGIN
    p.a := 100;
    C(y)
  END B;

BEGIN
  r := 0;
  p.a := 0;
  B(2)
END A;

BEGIN
  A(v, 3);
  WRITELN(v)
END nest2.
