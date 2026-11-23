MODULE C5;
FROM Q2 IMPORT T, Make;
VAR a, b: T;
BEGIN a := Make(); b := a; IF a = b THEN a := NIL END END C5.
