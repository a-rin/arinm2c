MODULE m5;
FROM InOut IMPORT WriteInt, WriteLn;
IMPORT InOut;
VAR i: LONGINT;

PROCEDURE Twice(v: LONGINT): LONGINT;
BEGIN
  RETURN v * 2
END Twice;

BEGIN
  i := 7;
  WriteInt(i * 6, 0);
  WriteLn;
  InOut.WriteInt(Twice(i), 5);
  InOut.WriteLn
END m5.
