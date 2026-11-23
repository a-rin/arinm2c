/* deps: arinm2c -M Main.mod writes a Makefile that builds the program */
#ifndef ARINM2C_DEPS_H
#define ARINM2C_DEPS_H

#include "module.h"

/* write the Makefile of the program (or implementation module) srcname to out */
int deps_main(void);

#endif
