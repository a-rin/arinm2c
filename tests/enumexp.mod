MODULE enumexp;

MODULE Palette;
  EXPORT Shade, light, dark, Pick;
  TYPE Shade = (light, mid, dark);
  PROCEDURE Pick(s: Shade): LONGINT;
  BEGIN
    RETURN ORD(s) * 10
  END Pick;
END Palette;

VAR s: Shade;
BEGIN
  s := dark;
  WRITELN(Pick(s), ' ', Pick(light))
END enumexp.
