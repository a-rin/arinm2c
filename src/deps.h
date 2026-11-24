/* deps: arinm2c -M Main.mod writes a Makefile that builds the program */
#ifndef ARINM2C_DEPS_H
#define ARINM2C_DEPS_H

#include "module.h"

/* write the Makefile of the program (or implementation module) srcname to out */
int deps_main(void);

/* arinm2c -V a.c b.c ...: every module must have one version in all the files
   (each file lists the versions of the definition modules it was made with);
   returns 0 or 1 */
int versions_main(int n, char **files);

#endif
