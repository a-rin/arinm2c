MODULE m4;
VAR n: LONGINT;

PROCEDURE Fact(k: LONGINT): LONGINT;
BEGIN
  IF k <= 1 THEN
    RETURN 1
  END;
  RETURN k * Fact(k - 1)
END Fact;

PROCEDURE Show(v: LONGINT);
BEGIN
  WRITELN(v)
END Show;

PROCEDURE Sign(v: LONGINT): LONGINT;
BEGIN
  IF v < 0 THEN RETURN -1 ELSIF v = 0 THEN RETURN 0 END;
  RETURN 1
END Sign;

BEGIN
  n := 5;
  Show(Fact(n));
  Show(Sign(-7));
  Show(Sign(0));
  Show(Sign(3))
END m4.
