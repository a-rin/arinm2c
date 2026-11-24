IMPLEMENTATION MODULE Cnt;
FROM Base IMPORT N;
FROM Util IMPORT Twice;
VAR count: N;
PROCEDURE Add(n: N);
BEGIN
  count := count + Twice(n)
END Add;
PROCEDURE Get(): N;
BEGIN
  RETURN count
END Get;
BEGIN
  count := 0
END Cnt.
