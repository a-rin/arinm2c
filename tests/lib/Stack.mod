IMPLEMENTATION MODULE Stack;
VAR data: ARRAY [0..7] OF LONGINT;

PROCEDURE Push(v: LONGINT);
BEGIN
  data[top] := v;
  top := top + 1
END Push;

PROCEDURE Pop(): LONGINT;
BEGIN
  top := top - 1;
  RETURN data[top]
END Pop;

BEGIN
  top := 0
END Stack.
