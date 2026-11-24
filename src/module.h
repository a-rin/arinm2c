/* module: definition, implementation and program modules; IMPORT */
#ifndef ARINM2C_MODULE_H
#define ARINM2C_MODULE_H

#include "decl.h"

extern char **incdirs;

extern int nincdirs;

char *readfile(const char *path);

/* the path of name+ext in the directory of the source or in a -I directory */
char *findfile(const char *name, const char *ext);

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

/* the version of a definition module: the MD5 of the tokens of its text */
void def_md5(const char *text, char out[33]);

Module *load_module(const char *name);

void use_module(Module *md);

void alias(Sym *s);

/* the constants of an enumeration type come with the type (FROM m IMPORT t,
   EXPORT t); list holds the declarations of the module that has the type */
void alias_enum_consts(Sym *ts, Sym *list);

/* the constant s of an enumeration type has been imported or exported with its
   type already: naming it again is no error */
int already_aliased(Sym *s);

void def_module(const char *expected, int header);

void impl_module(void);

void module(void);

#endif
