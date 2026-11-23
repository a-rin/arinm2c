MODULE C1;
FROM Q2 IMPORT T, Make;
VAR a: T;
BEGIN a := Make(); a^.v := 9 END C1.
