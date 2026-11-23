/* types: the types of Modula-2 and the rules of the integer widths */
#include "arinm2c.h"

Type *mk(Kind k, int size, int sign, const char *cn)
{
    Type *t = calloc(1, sizeof *t);
    t->k = k;
    t->size = size;
    t->sign = sign;
    t->cname = (char *)cn;
    return t;
}

Type *H(Type *t) { return t->host ? t->host : t; }

Type *t_int8, *t_card8, *t_int16, *t_card16, *t_int32, *t_card32,
    *t_int64, *t_card64, *t_bool, *t_char, *t_z, *t_str, *t_nil, *t_byte,
    *t_addr, *t_setlit;

const char *tname(Type *t)
{
    switch (t->k) {
    case K_INT: return t->cname;
    case K_Z: return "an integer constant";
    case K_BOOL: return "BOOLEAN";
    case K_CHAR: return "CHAR";
    case K_STR: return "a string";
    case K_NIL: return "NIL";
    default: return t->cname ? t->cname : "a structured type";
    }
}

int isint(Type *t) { return t->k == K_INT || t->k == K_Z; }

/* the type of an arithmetic expression: widths must agree (VAL changes them) */
Type *arith(Type *a, Type *b)
{
    if (!isint(a) || !isint(b))
        die("integer operands expected (found %s and %s)", tname(a), tname(b));
    if (a->k == K_Z)
        return H(b);
    if (b->k == K_Z)
        return H(a);
    if (H(a) != H(b))
        die("mixed integer types %s and %s: use VAL", tname(a), tname(b));
    return H(a);
}
