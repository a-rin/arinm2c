MODULE d3;
FROM R IMPORT A, B, MakeA, MakeB;
VAR a: A; b: B;
BEGIN a := MakeA(); b := a END d3.
