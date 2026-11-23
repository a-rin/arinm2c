MODULE M11;
VAR total: INTEGER;

MODULE Counter;
  EXPORT Inc1, Get;
  VAR n: INTEGER;
  PROCEDURE Inc1;
  BEGIN
    n := n + 1
  END Inc1;
  PROCEDURE Get(): INTEGER;
  BEGIN
    RETURN n
  END Get;
BEGIN
  n := 10
END Counter;

MODULE Twice;
  EXPORT Double;
  PROCEDURE Double(v: INTEGER): INTEGER;
  BEGIN
    RETURN v * 2
  END Double;
END Twice;

BEGIN
  Inc1;
  Inc1;
  total := Double(Get());
  WRITELN(total)
END M11.
