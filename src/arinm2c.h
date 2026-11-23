/*
 * arinm2c: a Modula-2 (PIM4) to C translator.
 *
 * Supported: modules (definition, implementation, program, local), IMPORT,
 * CONST TYPE VAR PROCEDURE, all statements, integers with the width rules
 * (VAL changes a width, literals have the type Z), BOOLEAN, CHAR, enumeration,
 * subrange, SET, REAL and LONGREAL, POINTER, RECORD (variants), ARRAY (open
 * arrays of up to three dimensions), procedure types, nested procedures, opaque
 * types, WITH, SYSTEM (BYTE, ADDRESS, ADR).
 * See status/modula2_fpc.md in daily-ops for the decisions.
 *
 * One pass: the parser checks the types and writes the C text on the way.
 * The translator never frees memory (a one-shot process).
 * WRITE and WRITELN are built in as a test aid, as in the FPC prototype.
 *
 * The modules, in the order of their headers (a header includes the ones
 * before it):
 *   util      messages, C text formatting, the output
 *   lex       the scanner
 *   types     the types and the rules of the integer widths
 *   symbols   expressions, symbols, scopes, the predeclared names
 *   expr      expressions, designators, calls; assignment compatibility
 *   typedecl  the declaration of types
 *   stmt      statements
 *   decl      CONST TYPE VAR PROCEDURE, local modules
 *   module    .def .mod and program modules, IMPORT
 *   deps      -M: the Makefile of a program
 *   main      the command line
 */

#ifndef ARINM2C_H
#define ARINM2C_H

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deps.h"

#endif
