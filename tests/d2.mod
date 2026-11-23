MODULE D2;
FROM R IMPORT A, B, MakeA, MakeB;
VAR a: A; b: B;
BEGIN a := MakeB() END D2.
