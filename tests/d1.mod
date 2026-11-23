MODULE d1;
FROM R IMPORT A, B, Holder, MakeA, MakeB, Fill, Val;
VAR h: Holder;
BEGIN Fill(h); WRITELN(Val(h)) END d1.
