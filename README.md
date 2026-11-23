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
    cc -o main Main.c Stack.c

A module that has a definition module but no implementation module can be
written in C by hand; see `tests/lib/InOut.def` and `tests/lib/InOut.c`.
Every module with a body has a function `X_init()` that runs its body once; the
program calls the ones of the imported modules first.

## What it does

* Modules: definition, implementation, program and local modules (only the
  exported names are visible outside), `IMPORT`, `FROM ... IMPORT`, opaque types.
* Statements: all of those of PIM4, including `CASE`, `LOOP`/`EXIT`,
  `FOR ... BY` and `WITH`.
* Types: integers, `BOOLEAN`, `CHAR`, enumerations, subranges, `SET OF`,
  `POINTER`, `RECORD` (with variants), `ARRAY` (also open arrays), procedure
  types.
* `SYSTEM`: `BYTE`, `ADDRESS`, `ADR`. `ARRAY OF BYTE` accepts a variable of any
  type.

### Integer types

`INTEGER` and `CARDINAL` are 32 bits, `LONGINT` and `LONGCARD` are 64 bits.
`INT8` ... `INT64` and `CARD8` ... `CARD64` are predeclared. Types of different
widths (or of the same width and different signs) cannot be mixed in an
expression: `VAL(INT64, a)` converts. An integer constant has no width of its
own and fits any integer type.

### Built in for tests

`WRITE` and `WRITELN` print integers, characters, strings and booleans. They are
not part of Modula-2; they are there so that a test needs no library.

## Not done

`REAL`, nested procedures, typed set constructors, multi-dimensional open
arrays. The C code has the semantics of C where Modula-2 leaves the details to
the implementation: `MOD` of negative numbers, overflow of signed integers, and
no check of array indices.

## Source

`src/arinm2c.h` describes the modules. The tests in `tests/` give the expected
output of each program (`.out`) or the message of the error that the translator
must report (`.err`).

## License

MIT; see `LICENSE`.
