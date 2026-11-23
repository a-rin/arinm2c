/* expr: type checking and C text of expressions, designators and calls */
#ifndef ARINM2C_EXPR_H
#define ARINM2C_EXPR_H

#include "symbols.h"

char *numc(long long v);

Expr constint(long long v);

long long cval(Expr *e);

void check_assign(Type *dst, Expr *e, const char *what);

char *conv(Type *dst, Expr *e);

Expr call_with(Proc *p, const char *cfunc, const char *name,
                      int want_result, const char *link);

/* the C text of a variable of a procedure: in the frame, or of an enclosing
   procedure (through the static link _up) */
char *var_text(Sym *s);

/* the frame of the procedure at level pl, as the first argument of a call */
char *static_link(int pl);

Expr call(Sym *s, int want_result);

/* a record field named in an enclosing WITH */
typedef struct With {
    Type *rec;
    char *c;
} With;

extern With with_stack[16];

extern int nwith;

int with_field(const char *n, Expr *e);

Expr designator(Sym *s);

Expr designator_tail(Expr e);

char *ctype_of(Type *t);

Expr expr(void);

Expr constexpr_(void);

#endif
