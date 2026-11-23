MODULE addrbad1;
FROM SYSTEM IMPORT ADDRESS, ADR;
VAR a: ADDRESS; x: LONGINT;
BEGIN
  a := ADR(x);
  a := a + 1
END addrbad1.
