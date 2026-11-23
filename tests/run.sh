#!/bin/sh
# Each tests/NAME.mod is translated, compiled and run; the output must equal
# tests/NAME.out.  A tests/NAME.err file (a part of the message) means that the
# translation must fail with a message containing that text.
# A tests/NAME.rt file (a part of a message) means that the program must stop
# with a run time error and write a message that contains that text.
# tests/lib holds the modules the tests import: NAME.def (translated to a
# header), NAME.mod (translated to C), and hand written NAME.c for a definition
# module that has no implementation module.
cd "$(dirname "$0")/.." || exit 1
tmp=${TMPDIR:-/tmp}/arinm2c.$$
mkdir -p "$tmp"
fail=0; n=0
libs=""
for d in tests/lib/*.def; do
  b=$(basename "$d" .def)
  ./arinm2c "$d" -o "$tmp/$b.h" || { echo "FAIL lib $b.def"; exit 1; }
done
for m in tests/lib/*.mod; do
  b=$(basename "$m" .mod)
  ./arinm2c "$m" -o "$tmp/$b.c" || { echo "FAIL lib $b.mod"; exit 1; }
  libs="$libs $tmp/$b.c"
done
for c in tests/lib/*.c; do libs="$libs $c"; done
# tests/bad: modules that must be rejected (NAME.def, NAME.mod, NAME.err)
for m in tests/bad/*.mod; do
  b=$(basename "$m" .mod); n=$((n+1))
  if ./arinm2c -I tests/bad "$m" -o "$tmp/bad_$b.c" 2>"$tmp/bad_$b.msg"; then
    echo "FAIL bad/$b: translation should have failed"; fail=$((fail+1))
  elif grep -qF "$(cat "tests/bad/$b.err")" "$tmp/bad_$b.msg"; then
    echo "ok   bad/$b (rejected)"
  else
    echo "FAIL bad/$b: message was: $(cat "$tmp/bad_$b.msg")"; fail=$((fail+1))
  fi
done
for m in tests/*.mod; do
  b=$(basename "$m" .mod); n=$((n+1))
  if [ -f "tests/$b.err" ]; then
    if ./arinm2c -I tests/lib "$m" -o "$tmp/$b.c" 2>"$tmp/$b.msg"; then
      echo "FAIL $b: translation should have failed"; fail=$((fail+1))
    elif grep -qF "$(cat "tests/$b.err")" "$tmp/$b.msg"; then
      echo "ok   $b (rejected)"
    else
      echo "FAIL $b: message was: $(cat "$tmp/$b.msg")"; fail=$((fail+1))
    fi
    continue
  fi
  if ! ./arinm2c -I tests/lib "$m" -o "$tmp/$b.c" 2>"$tmp/$b.msg"; then
    echo "FAIL $b: $(cat "$tmp/$b.msg")"; fail=$((fail+1)); continue
  fi
  if ! ${CC:-cc} -std=c11 -fwrapv -Wall -Wno-unused-label -I "$tmp" -o "$tmp/$b" "$tmp/$b.c" $libs 2>"$tmp/$b.cc"; then
    echo "FAIL $b: C compile error"; head -5 "$tmp/$b.cc"; fail=$((fail+1)); continue
  fi
  if [ -f "tests/$b.rt" ]; then
    # a run time error: the program must stop with a message that has this text
    if "$tmp/$b" >"$tmp/$b.got" 2>"$tmp/$b.rterr"; then
      echo "FAIL $b: the program should have stopped"; fail=$((fail+1))
    elif ! grep -qF "$(cat "tests/$b.rt")" "$tmp/$b.rterr"; then
      echo "FAIL $b: message was: $(cat "$tmp/$b.rterr")"; fail=$((fail+1))
    else
      # with -DM2_ABORT the error must end the program with a signal (a core dump)
      ${CC:-cc} -std=c11 -fwrapv -DM2_ABORT -I "$tmp" -o "$tmp/${b}_a" "$tmp/$b.c" $libs 2>/dev/null
      (ulimit -c 0; "$tmp/${b}_a" >/dev/null 2>&1; echo $? >"$tmp/$b.rc") 2>/dev/null
      rc=$(cat "$tmp/$b.rc")
      if [ "$rc" -ge 128 ]; then echo "ok   $b (run time error, abort with -DM2_ABORT)"
      else echo "FAIL $b: -DM2_ABORT gave the exit status $rc"; fail=$((fail+1)); fi
    fi
    continue
  fi
  "$tmp/$b" >"$tmp/$b.got"
  if cmp -s "$tmp/$b.got" "tests/$b.out"; then echo "ok   $b"
  else echo "FAIL $b: output differs"; diff "$tmp/$b.got" "tests/$b.out" | head -5; fail=$((fail+1)); fi
done
rm -rf "$tmp"
echo "$n tests, $fail failed"
[ "$fail" = 0 ]
