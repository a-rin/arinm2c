MODULE addr;
FROM SYSTEM IMPORT ADDRESS, ADR;
TYPE P = POINTER TO LONGINT;
VAR a: ADDRESS; p: P; x: LONGINT; n: CARD64;
BEGIN
  x := 5;
  a := ADR(x);
  p := a;
  p^ := p^ + 1;
  a := p;
  n := VAL(CARD64, a);
  IF n # 0 THEN a := VAL(ADDRESS, n) END;
  IF a = ADR(x) THEN WRITELN(x) END
END addr.
