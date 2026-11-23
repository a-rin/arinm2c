MODULE S1;
(* CONST, enumeration, subrange, ORD, INC, DEC *)
CONST N = 10; Greeting = "hi";
TYPE
  Color = (red, green, blue);
  Small = [0..9];
VAR c: Color; k: Small; i: INTEGER;
BEGIN
  c := green;
  k := 3;
  i := N + ORD(c) + k;
  INC(i);
  DEC(i, 2);
  WRITELN(i, ' ', Greeting)
END S1.
