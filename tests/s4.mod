MODULE s4;
(* procedure types *)
TYPE Fn = PROCEDURE (LONGINT): LONGINT;
VAR f: Fn;

PROCEDURE Twice(v: LONGINT): LONGINT;
BEGIN
  RETURN v * 2
END Twice;

PROCEDURE Apply(g: Fn; v: LONGINT): LONGINT;
BEGIN
  RETURN g(v)
END Apply;

BEGIN
  f := Twice;
  WRITELN(f(3), ' ', Apply(Twice, 5))
END s4.
