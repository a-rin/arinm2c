/* symbols: expressions, symbols, scopes and the predeclared names */
#ifndef ARINM2C_SYMBOLS_H
#define ARINM2C_SYMBOLS_H

#include "types.h"

typedef struct Expr {
    char *c;            /* the C text */
    Type *t;
    int isconst;        /* an integer constant with the value v */
    long long v;
    int lvalue;
    int istype;         /* a type name (only as an argument of SIZEOF, VAL) */
    char *sraw;         /* the bytes of a string or CHAR constant */
    char *high;         /* an open array: the C text of HIGH */
} Expr;

typedef struct Param {
    char *name;
    Type *t;
    int isvar;
} Param;

typedef struct Proc {
    int n;
    Param p[16];
    Type *ret;
} Proc;

typedef enum { S_VAR, S_CONST, S_TYPE, S_PROC, S_MODULE } SKind;

struct Module;

typedef struct Sym {
    char *name;
    SKind k;
    Type *t;
    char *cname;
    int isvarparam;
    Expr ce;            /* S_CONST */
    Proc *proc;         /* S_PROC */
    struct Module *mod; /* S_MODULE */
    int exported;       /* declared in a definition module */
    int defined;        /* an exported procedure that has its body */
    int imported;       /* copied by FROM m IMPORT x */
    struct Sym *next;
} Sym;

typedef struct Module {
    char *name;
    Sym *syms;          /* what the definition module declares */
    int hasimpl;        /* an implementation module (.mod) exists */
    int builtin;        /* SYSTEM */
    struct Module *next;
} Module;

extern Sym *globals, *locals, *predef;

extern int inproc;

extern char *modname;

extern Proc *curproc;

extern int indef;

extern int impl_mode;

extern int mainbody;

extern Module *mods;

Sym *lookup_in(Sym *l, const char *n);

Sym *find(const char *n);

Sym *resolve(Sym *s);

Sym *declare(const char *name, SKind k, Type *t);

void init_predef(void);

#endif
