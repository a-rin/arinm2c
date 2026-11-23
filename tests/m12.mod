MODULE M12;
FROM Q IMPORT Make, Put, Get;
IMPORT Q;
VAR a, b: Q.T;
BEGIN
  a := Make();
  b := Make();
  Put(a, 42);
  Put(b, 7);
  WRITELN(Get(a), ' ', Get(b))
END M12.
