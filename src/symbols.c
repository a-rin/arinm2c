/* symbols: expressions, symbols, scopes and the predeclared names */
#include "arinm2c.h"

Sym *globals, *locals, *predef;

int inproc;

char *modname;

Proc *curproc;

int indef;       /* reading a definition module */

int impl_mode;   /* translating an implementation module */

int mainbody;    /* the module body is main (a program module) */

Module *mods;

Sym *lookup_in(Sym *l, const char *n)
{
    for (; l; l = l->next)
        if (!strcmp(l->name, n))
            return l;
    return NULL;
}

Sym *find(const char *n)
{
    Sym *s = NULL;
    if (inproc)
        s = lookup_in(locals, n);
    if (!s)
        s = lookup_in(globals, n);
    if (!s)
        s = lookup_in(predef, n);
    return s;
}

/* IMPORT m: the name m.x is looked up in the module m */
Sym *resolve(Sym *s)
{
    while (s && s->k == S_MODULE) {
        Sym *m;
        char *n;
        expect(".");
        n = ident();
        for (m = s->mod->syms; m; m = m->next)
            if (!m->imported && !strcmp(m->name, n))
                break;
        if (!m)
            die("module %s has no '%s'", s->name, n);
        s = m;
    }
    return s;
}

Sym *declare(const char *name, SKind k, Type *t)
{
    Sym *s, **list = inproc ? &locals : &globals;
    if (lookup_in(*list, name))
        die("'%s' is declared twice", name);
    s = calloc(1, sizeof *s);
    s->name = (char *)name;
    s->k = k;
    s->t = t;
    s->cname = inproc ? fmt("v_%s", name) : fmt("%s_%s", modname, name);
    s->exported = indef;
    s->next = *list;
    *list = s;
    return s;
}

static void pre(const char *name, SKind k, Type *t)
{
    Sym *s = calloc(1, sizeof *s);
    s->name = (char *)name;
    s->k = k;
    s->t = t;
    s->next = predef;
    predef = s;
}

static void pre_const(const char *name, Type *t, long long v, const char *c)
{
    pre(name, S_CONST, t);
    predef->ce.t = t;
    predef->ce.isconst = (t == t_z);
    predef->ce.v = v;
    predef->ce.c = (char *)c;
}

void init_predef(void)
{
    t_int8 = mk(K_INT, 1, 1, "int8_t");
    t_card8 = mk(K_INT, 1, 0, "uint8_t");
    t_int16 = mk(K_INT, 2, 1, "int16_t");
    t_card16 = mk(K_INT, 2, 0, "uint16_t");
    t_int32 = mk(K_INT, 4, 1, "int32_t");
    t_card32 = mk(K_INT, 4, 0, "uint32_t");
    t_int64 = mk(K_INT, 8, 1, "int64_t");
    t_card64 = mk(K_INT, 8, 0, "uint64_t");
    t_bool = mk(K_BOOL, 1, 0, "uint8_t");
    t_char = mk(K_CHAR, 1, 0, "char");
    t_z = mk(K_Z, 0, 1, "int64_t");
    t_str = mk(K_STR, 0, 0, "const char *");
    t_nil = mk(K_NIL, 8, 0, "void *");
    t_byte = mk(K_INT, 1, 0, "uint8_t");
    t_addr = mk(K_PTR, 8, 0, "void *");
    t_addr->isaddr = 1;
    t_setlit = mk(K_SETLIT, 8, 0, "uint64_t");
    {
        Type *bs = mk(K_SET, 8, 0, "uint64_t");
        bs->base = t_card32;
        bs->slo = 0;
        bs->shi = 31;
        pre("BITSET", S_TYPE, bs);
    }
    pre("INTEGER", S_TYPE, t_int32);
    pre("CARDINAL", S_TYPE, t_card32);
    pre("LONGINT", S_TYPE, t_int64);
    pre("LONGCARD", S_TYPE, t_card64);
    pre("INT8", S_TYPE, t_int8);
    pre("INT16", S_TYPE, t_int16);
    pre("INT32", S_TYPE, t_int32);
    pre("INT64", S_TYPE, t_int64);
    pre("CARD8", S_TYPE, t_card8);
    pre("CARD16", S_TYPE, t_card16);
    pre("CARD32", S_TYPE, t_card32);
    pre("CARD64", S_TYPE, t_card64);
    pre("BOOLEAN", S_TYPE, t_bool);
    pre("CHAR", S_TYPE, t_char);
    pre_const("TRUE", t_bool, 1, "1");
    pre_const("FALSE", t_bool, 0, "0");
    pre_const("NIL", t_nil, 0, "NULL");
    {
        /* the module SYSTEM is built in */
        Module *m = calloc(1, sizeof *m);
        const char *names[3] = {"BYTE", "ADDRESS", "ADR"};
        SKind kinds[3] = {S_TYPE, S_TYPE, S_PROC};
        Type *types[3];
        int i;
        types[0] = t_byte;
        types[1] = t_addr;
        types[2] = NULL;
        m->name = "SYSTEM";
        m->builtin = 1;
        for (i = 0; i < 3; i++) {
            Sym *sy = calloc(1, sizeof *sy);
            sy->name = (char *)names[i];
            sy->k = kinds[i];
            sy->t = types[i];
            sy->exported = 1;
            sy->next = m->syms;
            m->syms = sy;
        }
        m->next = mods;
        mods = m;
    }
}
