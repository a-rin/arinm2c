MODULE ltest;
FROM Cnt IMPORT Add, Get;
BEGIN
  Add(3);
  Add(4);
  WRITELN(Get())
END ltest.
