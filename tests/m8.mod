MODULE M8;
TYPE
  P = POINTER TO Rec;
  Rec = RECORD v: LONGINT; next: P END;
  Vec = ARRAY [0..4] OF LONGINT;
VAR i, s: LONGINT; v: Vec; p, q: P;
BEGIN
  FOR i := 0 TO 4 DO v[i] := i * i END;
  s := 0;
  FOR i := 0 TO 4 BY 2 DO s := s + v[i] END;
  WRITELN(s);
  FOR i := 4 TO 0 BY -1 DO WRITE(v[i], ' ') END;
  WRITELN;
  FOR i := 10 TO 1 BY -3 DO WRITE(i, ' ') END;
  WRITELN;
  i := 0;
  LOOP
    IF i >= 3 THEN EXIT END;
    i := i + 1
  END;
  WRITELN(i);
  FOR i := 0 TO 6 DO
    CASE i OF
      1, 2: WRITE('a')
    | 3..4: WRITE('b')
    | 5: WRITE('c')
    ELSE WRITE('.')
    END
  END;
  WRITELN;
  NEW(p);
  p^.v := 7;
  NEW(q);
  q^.v := 8;
  q^.next := NIL;
  p^.next := q;
  WRITELN(p^.v + p^.next^.v);
  WRITELN("hello", ' ', "x")
END M8.
