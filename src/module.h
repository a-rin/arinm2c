/* module: definition, implementation and program modules; IMPORT */
#ifndef ARINM2C_MODULE_H
#define ARINM2C_MODULE_H

#include "decl.h"

extern char *incdirs[16];

extern int nincdirs;

char *readfile(const char *path);

typedef struct Saved {
    const char *src, *srcname;
    size_t pos;
    int curline, errline, inproc, tcount, quiet, indef, impl_mode, mainbody;
    Tok tok;
    Sym *globals, *locals;
    char *modname;
    Proc *curproc;
    Fwd *fwds;
} Saved;

Module *load_module(const char *name);

void use_module(Module *md);

void alias(Sym *s);

void def_module(const char *expected, int header);

void impl_module(void);

void module(void);

#endif
