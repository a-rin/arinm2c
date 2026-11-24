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
# arinm2c -M writes a Makefile; make builds the program from it, a second make
# finds nothing to do, and a changed .def makes make build again
root=$(pwd)
for b in m10 m5 chain; do
  n=$((n+1)); d="$tmp/mk_$b"; mkdir "$d" "$d/lib"; cp tests/lib/* "$d/lib/"
  if ! ./arinm2c -I "$d/lib" -M "$root/tests/$b.mod" -o "$d/Makefile" 2>"$d/err"; then
    echo "FAIL make/$b: -M: $(cat "$d/err")"; fail=$((fail+1)); continue
  fi
  if ! (cd "$d" && make M2C="$root/arinm2c" >log 2>&1); then
    echo "FAIL make/$b: make failed"; tail -5 "$d/log"; fail=$((fail+1)); continue
  fi
  (cd "$d" && "./$b" >got)
  if ! cmp -s "$d/got" "tests/$b.out"; then
    echo "FAIL make/$b: output differs"; fail=$((fail+1)); continue
  fi
  if ! (cd "$d" && make -q M2C="$root/arinm2c" "$b" >/dev/null 2>&1); then
    echo "FAIL make/$b: a second make finds something to do"; fail=$((fail+1)); continue
  fi
  echo "ok   make/$b"
done
# a change of a definition module two imports away makes make build again
d="$tmp/mk_chain"; n=$((n+1))
sleep 1; touch "$d/lib/Cb.def"
if (cd "$d" && make -q M2C="$root/arinm2c" chain >/dev/null 2>&1); then
  echo "FAIL make/chain: a changed Cb.def does not make make build again"; fail=$((fail+1))
elif ! (cd "$d" && make M2C="$root/arinm2c" >log 2>&1 && ./chain | cmp -s - "$root/tests/chain.out"); then
  echo "FAIL make/chain: the build after the change"; fail=$((fail+1))
else echo "ok   make/chain (a changed .def builds again)"; fi
# a circular import is found by -M, too
n=$((n+1))
if ./arinm2c -I tests/bad -M tests/bad/cyc.mod -o "$tmp/cyc.mk" 2>"$tmp/cyc.err"; then
  echo "FAIL make/cyc: -M should have failed"; fail=$((fail+1))
elif grep -qF "circular import" "$tmp/cyc.err"; then echo "ok   make/cyc (rejected)"
else echo "FAIL make/cyc: $(cat "$tmp/cyc.err")"; fail=$((fail+1)); fi
n=$((n+1))
if ${CC:-cc} -o "$tmp/md5test" tests/md5test.c src/md5.c && "$tmp/md5test"; then echo "ok   md5"
else echo "FAIL md5"; fail=$((fail+1)); fi
# the version of a definition module: objects made with the same version link
# (also when only comments or the layout differ), a different version does not
vbuild() {  # vbuild DIR_OF_Vx.def DIR_OF_Vx.def_for_the_client: link, status
  ./arinm2c -I "$1" tests/ver/Vx.mod -o "$tmp/vx_impl.c" &&
  ./arinm2c -I "$2" tests/ver/vmain.mod -o "$tmp/vmain.c" &&
  mkdir -p "$tmp/vh1" "$tmp/vh2" &&
  ./arinm2c -I tests/ver "$1/Vx.def" -o "$tmp/vh1/Vx.h" &&
  ./arinm2c -I tests/ver "$2/Vx.def" -o "$tmp/vh2/Vx.h" &&
  ${CC:-cc} -std=c11 -fwrapv -I "$tmp/vh1" -c "$tmp/vx_impl.c" -o "$tmp/Vx.o" &&
  ${CC:-cc} -std=c11 -fwrapv -I "$tmp/vh2" -c "$tmp/vmain.c" -o "$tmp/vmain.o" &&
  ${CC:-cc} -o "$tmp/vmain" "$tmp/vmain.o" "$tmp/Vx.o" 2>"$tmp/vlink.err"
}
n=$((n+1))
if vbuild tests/ver/v1 tests/ver/v1 && [ "$("$tmp/vmain")" = 2 ]; then echo "ok   ver/same"
else echo "FAIL ver/same: the same version does not link"; fail=$((fail+1)); fi
n=$((n+1))
if vbuild tests/ver/v1 tests/ver/v1c && [ "$("$tmp/vmain")" = 2 ]; then echo "ok   ver/layout (comments and layout are not a version)"
else echo "FAIL ver/layout: a changed comment is a new version"; fail=$((fail+1)); fi
n=$((n+1))
if vbuild tests/ver/v1 tests/ver/v2; then echo "FAIL ver/mixed: two versions linked"; fail=$((fail+1))
elif grep -q "Vx__def_" "$tmp/vlink.err"; then echo "ok   ver/mixed (the link fails)"
else echo "FAIL ver/mixed: $(head -3 "$tmp/vlink.err")"; fail=$((fail+1)); fi
# A.def has no code (only a type), B.def imports A, cmain imports A: B and cmain
# must agree on the version of A (arinm2c -V); the linker cannot see it
n=$((n+1))
./arinm2c -I tests/ver2/a1 tests/ver2/B.mod -o "$tmp/vB.c" &&
./arinm2c -I tests/ver2/a1 tests/ver2/cmain.mod -o "$tmp/vc1.c" &&
./arinm2c -I tests/ver2/a2 tests/ver2/cmain.mod -o "$tmp/vc2.c" || { echo "FAIL ver2: translation"; exit 1; }
if ! ./arinm2c -V "$tmp/vB.c" "$tmp/vc1.c" 2>"$tmp/v2.err"; then
  echo "FAIL ver2/same: $(cat "$tmp/v2.err")"; fail=$((fail+1))
elif ./arinm2c -V "$tmp/vB.c" "$tmp/vc2.c" 2>"$tmp/v2.err"; then
  echo "FAIL ver2/mixed: two versions of A were accepted"; fail=$((fail+1))
elif grep -q "module A has two versions" "$tmp/v2.err"; then echo "ok   ver2 (a module without code: -V finds the mix)"
else echo "FAIL ver2/mixed: $(cat "$tmp/v2.err")"; fail=$((fail+1)); fi
# libraries: tb (Base: a type, Util: code) is built and installed, tl (Cnt, uses
# Util and Base, requires tb) is built and installed, ltest is built with -l tl
lt="$tmp/lib"; pfx="$lt/pfx"; mkdir -p "$lt/btb" "$lt/btl" "$lt/cl"
lstep() {  # lstep NAME COMMAND...: run, on failure say which step
  s=$1; shift
  "$@" >"$lt/step.log" 2>&1 || { echo "FAIL lib/$s: $(head -3 "$lt/step.log")"; return 1; }
}
lmake() { (cd "$1" && shift && make M2C="$root/arinm2c" "$@"); }
n=$((n+1))
if lstep tb-M "$root/arinm2c" -I "$root/tests/libt/tb" -M "$root/tests/libt/tb/tb.m2lib" -o "$lt/btb/Makefile" &&
   lstep tb-make lmake "$lt/btb" PREFIX="$pfx" install &&
   lstep tl-M "$root/arinm2c" -I "$root/tests/libt/tl" -L "$pfx/lib/m2" -M "$root/tests/libt/tl/tl.m2lib" -o "$lt/btl/Makefile" &&
   lstep tl-make lmake "$lt/btl" PREFIX="$pfx" install &&
   lstep ltest-M "$root/arinm2c" -L "$pfx/lib/m2" -l tl -M "$root/tests/libt/ltest.mod" -o "$lt/cl/Makefile" &&
   lstep ltest-make lmake "$lt/cl" &&
   (cd "$lt/cl" && ./ltest >got) && cmp -s "$lt/cl/got" tests/libt/ltest.out; then
  if [ -e "$lt/cl/Cnt.c" ] || [ -e "$lt/cl/Util.c" ] || [ -e "$lt/cl/Base.h" ]; then
    echo "FAIL lib: the program built a module of the library"; fail=$((fail+1))
  elif ! grep -q "^uses Base " "$pfx/lib/m2/tl/tl.m2lib" || ! grep -q "^uses Util " "$pfx/lib/m2/tl/tl.m2lib" || ! grep -q "^module Cnt " "$pfx/lib/m2/tl/tl.m2lib"; then
    echo "FAIL lib: the installed tl.m2lib has no versions"; fail=$((fail+1))
  else echo "ok   lib (build, install, use; a library that requires a library)"; fi
else echo "FAIL lib: see above"; fail=$((fail+1)); fi
# a module of a library that is also found in another directory
n=$((n+1))
if "$root/arinm2c" -I "$root/tests/libt/clash" -L "$pfx/lib/m2" -l tb -M tests/libt/ltest.mod -o "$lt/x.mk" 2>"$lt/clash.err"; then
  echo "FAIL lib/clash: accepted"; fail=$((fail+1))
elif grep -q "is also found" "$lt/clash.err"; then echo "ok   lib/clash (rejected)"
else echo "FAIL lib/clash: $(cat "$lt/clash.err")"; fail=$((fail+1)); fi
# tl was made with another version of Base than the one that is installed now
n=$((n+1))
sleep 1; cp tests/libt/Base2.def "$pfx/lib/m2/tb/Base.def"
if lmake "$lt/cl" >"$lt/mix.log" 2>&1; then
  echo "FAIL lib/mix: two versions of Base were accepted"; fail=$((fail+1))
elif grep -q "module Base has two versions" "$lt/mix.log"; then echo "ok   lib/mix (another version of a .def of the library is found)"
else echo "FAIL lib/mix: $(tail -3 "$lt/mix.log")"; fail=$((fail+1)); fi
# no small limit on the number of modules of a library (and of its imports)
n=$((n+1)); big="$tmp/big"; mkdir "$big"; printf 'library big\n' >"$big/big.m2lib"
i=1
while [ $i -le 70 ]; do
  printf 'DEFINITION MODULE M%s;\nTYPE T = INTEGER;\nEND M%s.\n' $i $i >"$big/M$i.def"
  printf 'module M%s\n' $i >>"$big/big.m2lib"; i=$((i+1))
done
if ./arinm2c -I "$big" -M "$big/big.m2lib" -o "$big/Makefile" 2>"$big/err" && grep -q "^M70.h:" "$big/Makefile"; then
  echo "ok   lib/70 modules"
else echo "FAIL lib/70 modules: $(cat "$big/err")"; fail=$((fail+1)); fi
# more than 16 libraries at the same time (-L, -l, the directories of the .def files)
n=$((n+1)); many="$tmp/many"; mkdir "$many"; i=1; largs=""
while [ $i -le 20 ]; do
  mkdir -p "$many/pfx/lib$i"
  printf 'library lib%s\nabi 1\nmodule Q%s\n' $i $i >"$many/pfx/lib$i/lib$i.m2lib"
  printf 'DEFINITION MODULE Q%s;\nTYPE T = INTEGER;\nEND Q%s.\n' $i $i >"$many/pfx/lib$i/Q$i.def"
  largs="$largs -l lib$i"; i=$((i+1))
done
printf 'MODULE mmain;\nFROM Q20 IMPORT T;\nVAR x: T;\nBEGIN\n  x := 1\nEND mmain.\n' >"$many/mmain.mod"
if ./arinm2c -L "$many/pfx" $largs -M "$many/mmain.mod" -o "$many/Makefile" 2>"$many/err" && grep -q -- "-llib20" "$many/Makefile"; then
  echo "ok   lib/20 libraries"
else echo "FAIL lib/20 libraries: $(cat "$many/err")"; fail=$((fail+1)); fi
rm -rf "$tmp"
echo "$n tests, $fail failed"
[ "$fail" = 0 ]
