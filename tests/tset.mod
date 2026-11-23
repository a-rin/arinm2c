MODULE tset;
TYPE Digits = SET OF [0..9];
VAR s: Digits; b: BITSET; n: LONGINT;
BEGIN
  s := Digits{1, 3, 5..7};
  b := BITSET{0, 31};
  n := 0;
  IF 6 IN s THEN n := n + 1 END;
  IF 31 IN b THEN n := n + 10 END;
  IF 2 IN s THEN n := n + 100 END;
  WRITELN(n)
END tset.
