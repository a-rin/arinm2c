MODULE s3;
(* sets with { } constructors *)
TYPE Digits = SET OF [0..9];
VAR s: Digits; n: LONGINT;
BEGIN
  s := {1, 3, 5..7};
  INCL(s, 9);
  EXCL(s, 3);
  n := 0;
  IF 5 IN s THEN n := n + 1 END;
  IF 3 IN s THEN n := n + 10 END;
  IF 9 IN s THEN n := n + 100 END;
  WRITELN(n)
END s3.
