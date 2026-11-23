# arinm2c

A translator from Modula-2 (PIM4) to C, written in C. It is a hobby project and
a work in progress.

The translator reads one module at a time and writes C. It checks the types as
it parses and reports the first error. The checks are not complete and have
not been tested against a large body of Modula-2 code.

## Build and test

    make          # builds ./arinm2c (needs a C11 compiler)
    make check    # translates, compiles and runs everything in tests/

## Use

The name of a module is the name of its file, with the same case: `Stack.def`,
`Stack.mod`, and `MODULE b1;` in `b1.mod`. The translator reports a difference.

A definition module `X.def` becomes a header, an implementation module `X.mod`
or a program module becomes a C file. `-I dir` adds a directory where the
definition modules of the imported modules are searched (the directory of the
source file is always searched).

    arinm2c -I lib Stack.def -o Stack.h
    arinm2c -I lib Stack.mod -o Stack.c
    arinm2c -I lib Main.mod  -o Main.c
    cc -fwrapv -o main Main.c Stack.c

### Build with make

`arinm2c -M` scans the imports (`IMPORT`, `FROM ... IMPORT`) of a program module
and writes a Makefile for it:

    arinm2c -I lib -M Main.mod > Makefile
    make                       # translates, compiles and links
    make CFLAGS="-std=c11 -fwrapv -g -DM2_ABORT"

The Makefile has a rule for every file: `X.def` to `X.h`, `X.mod` to `X.c`, the
C compiler for `X.c` and for a hand written `X.c` of a module that has no
implementation module, and the link of the program with the objects of all the
modules that the program needs (also those that only the implementation modules
import). A changed `.def` file builds again what depends on it. The rules are
plain (no pattern rules): they work with BSD make and GNU make. The paths are
those that you gave to `arinm2c -M`, so run `make` in the directory in which you
ran it. The generated files (`.h`, `.c`, `.o`) are written in the current
directory. A circular import is an error.

A module that has a definition module but no implementation module can be
written in C by hand; see `tests/lib/InOut.def` and `tests/lib/InOut.c`.
Every module with a body has a function `X_init()` that runs its body once; the
program calls the ones of the imported modules first.

## What it does

* Modules: definition, implementation, program and local modules (only the
  exported names are visible outside), `IMPORT`, `FROM ... IMPORT`, opaque types.
* Statements: all of those of PIM4, including `CASE`, `LOOP`/`EXIT`,
  `FOR ... BY` and `WITH`.
* Types: integers, `SHORTREAL`, `REAL` and `LONGREAL`, `BOOLEAN`, `CHAR`, enumerations,
  subranges, `SET OF`, `POINTER`, `RECORD` (with variants), `ARRAY` (also open
  arrays, up to three dimensions), procedure types.
* Procedures may be nested. C has no nested functions: a procedure that has
  nested procedures keeps its variables in a struct (a frame), and the C
  function of a nested procedure gets a pointer to the frame of the procedure
  around it. A nested procedure cannot be used as a value.
* `SYSTEM`: `BYTE`, `ADDRESS`, `ADR`. `ARRAY OF BYTE` accepts a variable of any
  type.

### Integer types

`INTEGER` and `CARDINAL` are 32 bits, `LONGINT` and `LONGCARD` are 64 bits.
`INT8` ... `INT64` and `CARD8` ... `CARD64` are predeclared. Types of different
widths (or of the same width and different signs) cannot be mixed in an
expression: `VAL(INT64, a)` converts. An integer constant has no width of its
own and fits any integer type.

### Versions of definition modules

The header of a module that has code (`X.mod` or a hand written `X.c`) has the
MD5 of its definition module (of its tokens: a changed comment or layout is not
a new version) as `X_DEF_MD5`. Every C file that includes `X.h` refers to a
symbol `X__def_<md5>`, and only the implementation defines it (a hand written
`X.c` writes `#define M2_IMPLEMENTS_X` before it includes `X.h`). So objects
and libraries made with two different versions of `X.def` do not link: the
linker reports the undefined `X__def_<md5>`.

### Real numbers

`SHORTREAL` is a C `float`, `REAL` a `double`, `LONGREAL` a `long double` (the
width of a `long double` depends on the platform: 80 bits on x86, 128 on
aarch64 Linux). As for the integers, the three cannot be mixed in an expression:
`VAL(LONGREAL, x)` converts. A real literal (`2.5`, `1.5E3`) fits all of them.
`FLOAT(i)` makes a `REAL` of an integer and `TRUNC(x)` an integer of a real number.

### Built in for tests

`WRITE` and `WRITELN` print integers, characters, strings and booleans. They are
not part of Modula-2; they are there so that a test needs no library.

## Not done

Arrays of more than three open dimensions, and Modula-2 features that are not
listed above.

## What the C code decides

* **Run time errors** stop the program with a message that has the file and the
  line of the Modula-2 source (the exit status is 1): an array index outside of
  the bounds, a division by zero (`DIV`, `MOD`), a `CASE` without a matching label
  and without `ELSE`, `HALT`, and no memory for `NEW`. Compile the C with
  `-DM2_ABORT` to call `abort()` instead of `exit(1)`: the program then ends with
  a signal and leaves a core dump (see `ulimit -c`) for a debugger. Compile with
  `-DM2_NOCHECK` to remove the checks of the indices and of the divisors.
* **`DIV` and `MOD`** are `/` and `%` of C. The Report (PIM4, 8.2.1) says that
  `x DIV y` is the truncated quotient and that
  `x = (x DIV y)*y + (x MOD y)` with `0 <= (x MOD y) < y`; for a negative `x`
  these two statements disagree, so the Report defines the operators for
  non-negative operands only. For a negative operand the C code does what C does:
  `-7 DIV 3 = -2`, `-7 MOD 3 = -1`. A division by zero is a run time error (see above).
* **Overflow of a signed integer** is undefined in C. Compile the C with
  `-fwrapv`: the value then wraps. There is no run time error for an overflow.
* **A value assigned to a subrange variable** is not checked at run time (a
  constant is checked when it is translated).

## Source

`src/arinm2c.h` describes the modules. The tests in `tests/` give the expected
output of each program (`.out`) or the message of the error that the translator
must report (`.err`).

## License

MIT; see `LICENSE`.
