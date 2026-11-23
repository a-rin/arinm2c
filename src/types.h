/* types: the types of Modula-2 and the rules of the integer widths */
#ifndef ARINM2C_TYPES_H
#define ARINM2C_TYPES_H

#include "lex.h"

typedef enum { K_INT, K_BOOL, K_CHAR, K_PTR, K_REC, K_ARR, K_Z, K_STR,
               K_NIL, K_FWD, K_ENUM, K_OPEN, K_SET, K_SETLIT, K_PROC } Kind;

typedef struct Type Type;

typedef struct Field {
    char *name;
    Type *t;
    struct Field *next;
} Field;

struct Type {
    Kind k;
    int size, sign;     /* K_INT */
    char *cname;
    Type *base;         /* K_PTR, K_ARR */
    long long lo, hi;   /* K_ARR */
    Field *fields;      /* K_REC */
    Type *host;         /* a subrange: the type it is a part of */
    int sub;            /* a subrange */
    long long slo, shi; /* subrange, enumeration and set element range */
    int isaddr;         /* ADDRESS */
    int copaque;        /* an opaque pointer: C sees a pointer to an unknown struct */
    struct Proc *sig;   /* K_PROC */
};

Type *mk(Kind k, int size, int sign, const char *cn);

Type *H(Type *t);

extern Type *t_int8, *t_card8, *t_int16, *t_card16, *t_int32, *t_card32, *t_int64, *t_card64, *t_bool, *t_char, *t_z, *t_str, *t_nil, *t_byte, *t_addr, *t_setlit;

const char *tname(Type *t);

int isint(Type *t);

Type *arith(Type *a, Type *b);

/* ARRAY OF ARRAY OF T: the number of dimensions, and T */
int open_dims(Type *t);
Type *open_leaf(Type *t);
/* the C text of the name of the high bound of dimension k of the parameter c */
char *high_name(const char *c, int k);
/* C parameters of a parameter of an open array type */
char *open_params(Type *t, const char *name, int isvar);
/* the same for a procedure type (types only) */
char *open_types(Type *t);

#endif
