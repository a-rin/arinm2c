MODULE M10;
FROM Stack IMPORT Push, Pop, top;
IMPORT InOut;
VAR a: LONGINT;
BEGIN
  Push(10); Push(20); Push(30);
  InOut.WriteInt(top, 0);
  InOut.WriteLn;
  a := Pop();
  InOut.WriteInt(a, 3);
  a := Pop();
  InOut.WriteInt(a, 3);
  a := Pop();
  InOut.WriteInt(a, 3);
  InOut.WriteLn;
  InOut.WriteInt(top, 0);
  InOut.WriteLn
END M10.
