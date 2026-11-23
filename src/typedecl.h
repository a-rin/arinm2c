/* typedecl: the declaration of types: record, array, set, pointer, ... */
#ifndef ARINM2C_TYPEDECL_H
#define ARINM2C_TYPEDECL_H

#include "expr.h"

extern int tcount;

typedef struct Fwd {
    char *name;
    Type *t;
    struct Fwd *next;
} Fwd;

extern Fwd *fwds;

Type *pointer_target(void);

Type *type_(const char *hint);

#endif
