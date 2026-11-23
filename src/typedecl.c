/* typedecl: the declaration of types: record, array, set, pointer, ... */
#include "arinm2c.h"

int tcount;

static char *autoname(void)
{
    return fmt("%s_%c%d", modname, indef ? 't' : 'l', ++tcount);
}

Fwd *fwds;

static void field_list(Type *t, Field ***tail);

/* CASE [tag:] type OF labels: fields | ... ELSE fields END */
static void variant_part(Type *t, Field ***tail)
{
    Type *tt;
    char *n, *tag = NULL;
    expect("CASE");
    n = ident();
    if (accept(":")) {
        tag = n;
        tt = type_(NULL);
    } else {
        Sym *sy = find(n);
        if (!sy || sy->k != S_TYPE)
            die("'%s' is not a type", n);
        tt = sy->t;
    }
    if (tag) {
        Field *fl = calloc(1, sizeof *fl);
        fl->name = tag;
        fl->t = tt;
        **tail = fl;
        *tail = &fl->next;
        emit("%s f_%s;", tt->cname, tag);
    }
    expect("OF");
    emit("union {");
    ind++;
    for (;;) {
        Field **before;
        if (accept("|"))
            continue;
        if (is("END"))
            break;
        if (!accept("ELSE")) {
            do {
                constexpr_();
                if (accept(".."))
                    constexpr_();
            } while (accept(","));
            expect(":");
        }
        emit("struct {");
        ind++;
        before = *tail;
        field_list(t, tail);
        if (*tail == before)
            emit("char _empty;");
        ind--;
        emit("};");
    }
    ind--;
    emit("};");
    expect("END");
}

static void field_list(Type *t, Field ***tail)
{
    while (!is("END") && !is("|") && !is("ELSE")) {
        char *names[16];
        int n = 0, i;
        Type *ft;
        if (accept(";"))
            continue;
        if (is("CASE")) {
            variant_part(t, tail);
            continue;
        }
        do {
            if (n >= 16)
                die("too many fields in one declaration");
            names[n++] = ident();
        } while (accept(","));
        expect(":");
        ft = type_(NULL);
        for (i = 0; i < n; i++) {
            Field *fl = calloc(1, sizeof *fl);
            fl->name = names[i];
            fl->t = ft;
            **tail = fl;
            *tail = &fl->next;
            emit("%s f_%s;", ft->cname, names[i]);
        }
    }
}

static Type *record_type(const char *name)
{
    Type *t;
    Field **tail;
    Fwd **fp;
    char *cn = name ? fmt("%s_%s", modname, name) : autoname();
    t = NULL;
    for (fp = &fwds; *fp; fp = &(*fp)->next)
        if (name && !strcmp((*fp)->name, name)) {
            t = (*fp)->t;
            *fp = (*fp)->next;
            break;
        }
    if (!t) {
        t = mk(K_REC, 0, 0, cn);
        emit("typedef struct %s %s;", cn, cn);
    }
    t->k = K_REC;
    tail = &t->fields;
    emit("struct %s {", cn);
    ind++;
    field_list(t, &tail);
    expect("END");
    ind--;
    emit("};");
    if (!t->fields)
        die("an empty record is not supported");
    return t;
}

/* the range of the values of an ordinal type (an array index, a set element) */
static void range_of(Type *t, long long *lo, long long *hi)
{
    if (t->k == K_ENUM || t->sub) {
        *lo = t->slo;
        *hi = t->shi;
    } else if (t->k == K_CHAR) {
        *lo = 0;
        *hi = 255;
    } else if (t->k == K_BOOL) {
        *lo = 0;
        *hi = 1;
    } else
        die("%s is not a subrange or an enumeration", tname(t));
}

static Type *make_sub(Expr lo, Expr hi)
{
    Type *host, *t;
    long long a = cval(&lo), b = cval(&hi);
    if (lo.t->k == K_CHAR && hi.t->k == K_CHAR)
        host = t_char;
    else if (lo.t->k == K_ENUM && hi.t->k == K_ENUM && H(lo.t) == H(hi.t))
        host = H(lo.t);
    else if (isint(lo.t) && isint(hi.t))
        host = a >= 0 ? (b > 4294967295LL ? t_card64 : t_card32)
                      : (a < -2147483647LL - 1 || b > 2147483647LL ? t_int64
                                                                   : t_int32);
    else
        die("bad bounds of a subrange");
    if (a > b)
        die("empty subrange %lld..%lld", a, b);
    t = calloc(1, sizeof *t);
    *t = *host;
    t->host = host;
    t->sub = 1;
    t->slo = a;
    t->shi = b;
    return t;
}

/* the type after POINTER TO: a type name (declared later: a forward
   reference), or a type */
Type *pointer_target(void)
{
    Type *b;
    if (tok.k == T_ID) {
        char *n = tok.s;
        Sym *s = find(n);
        if (s) {
            b = type_(NULL);
        } else {
            Fwd *f;
            next();
            for (f = fwds; f && strcmp(f->name, n); f = f->next)
                ;
            if (!f) {
                f = calloc(1, sizeof *f);
                f->name = n;
                f->t = mk(K_FWD, 0, 0, fmt("%s_%s", modname, n));
                f->next = fwds;
                fwds = f;
                emit("typedef struct %s %s;", f->t->cname, f->t->cname);
            }
            b = f->t;
        }
    } else
        b = type_(NULL);
    return b;
}

Type *type_(const char *hint)
{
    Type *t;
    if (accept("SET")) {
        Type *el;
        long long lo, hi;
        expect("OF");
        el = type_(NULL);
        range_of(el, &lo, &hi);
        if (lo < 0 || hi > 63)
            die("a set can hold the values 0..63 only");
        t = mk(K_SET, 8, 0, "uint64_t");
        t->base = el;
        t->slo = lo;
        t->shi = hi;
        return t;
    }
    if (accept("PROCEDURE")) {
        Proc *pr = calloc(1, sizeof *pr);
        char *params = NULL, *cn;
        int i;
        if (accept("(")) {
            if (!is(")")) {
                do {
                    int isvar = accept("VAR");
                    if (pr->n >= 16)
                        die("too many parameters");
                    pr->p[pr->n].name = "";
                    pr->p[pr->n].t = type_(NULL);
                    pr->p[pr->n].isvar = isvar;
                    pr->n++;
                } while (accept(",") || accept(";"));
            }
            expect(")");
            if (accept(":"))
                pr->ret = type_(NULL);
        }
        for (i = 0; i < pr->n; i++) {
            char *one = pr->p[i].t->k == K_OPEN
                            ? open_types(pr->p[i].t)
                            : pr->p[i].isvar ? fmt("%s *", pr->p[i].t->cname)
                                             : pr->p[i].t->cname;
            params = fmt("%s%s%s", params ? params : "", i ? ", " : "", one);
        }
        cn = hint ? fmt("%s_%s", modname, hint) : autoname();
        emit("typedef %s (*%s)(%s);", pr->ret ? pr->ret->cname : "void", cn,
             params ? params : "void");
        t = mk(K_PROC, 8, 0, cn);
        t->sig = pr;
        return t;
    }
    if (accept("(")) {
        char *cn = hint ? fmt("%s_%s", modname, hint) : autoname();
        long long n = 0;
        t = mk(K_ENUM, 4, 1, cn);
        emit("typedef int32_t %s;", cn);
        do {
            Sym *s = declare(ident(), S_CONST, t);
            s->ce.t = t;
            s->ce.isconst = 1;
            s->ce.v = n;
            s->ce.c = numc(n);
            n++;
        } while (accept(","));
        expect(")");
        t->slo = 0;
        t->shi = n - 1;
        return t;
    }
    if (accept("[")) {
        Expr a = constexpr_(), b;
        expect("..");
        b = constexpr_();
        expect("]");
        return make_sub(a, b);
    }
    if (tok.k == T_ID) {
        char *n = ident();
        Sym *s = resolve(find(n));
        if (!s || s->k != S_TYPE)
            die("'%s' is not a type", n);
        return s->t;
    }
    if (accept("RECORD"))
        return record_type(hint);
    if (accept("POINTER")) {
        Type *b;
        char *cn = hint ? fmt("%s_%s", modname, hint) : autoname();
        expect("TO");
        b = pointer_target();
        t = mk(K_PTR, 8, 0, cn);
        t->base = b;
        emit("typedef %s *%s;", b->cname, cn);
        return t;
    }
    if (accept("ARRAY")) {
        long long lo[8], hi[8];
        if (accept("OF")) {
            Type *b = type_(NULL);
            t = mk(K_OPEN, 8, 0, fmt("%s *", open_leaf(b)->cname));
            t->base = b;
            if (open_dims(t) > 3)
                die("an open array can have 3 dimensions at most");
            return t;
        }
        int n = 0, i;
        Type *el;
        char *cn;
        do {
            if (n >= 8)
                die("too many array dimensions");
            if (accept("[")) {
                Expr a, b;
                a = constexpr_();
                expect("..");
                b = constexpr_();
                expect("]");
                if (!isint(a.t) && a.t->k != K_CHAR && a.t->k != K_ENUM)
                    die("array bounds must be constants of an ordinal type");
                lo[n] = cval(&a);
                hi[n] = cval(&b);
            } else {
                range_of(type_(NULL), &lo[n], &hi[n]);
            }
            if (hi[n] < lo[n])
                die("empty array range");
            n++;
        } while (accept(","));
        expect("OF");
        el = type_(NULL);
        for (i = n - 1; i >= 0; i--) {
            cn = (i == 0 && hint) ? fmt("%s_%s", modname, hint) : autoname();
            t = mk(K_ARR, 0, 0, cn);
            t->base = el;
            t->lo = lo[i];
            t->hi = hi[i];
            emit("typedef struct { %s a[%lld]; } %s;", el->cname,
                 hi[i] - lo[i] + 1, cn);
            el = t;
        }
        return el;
    }
    die("type expected, found '%s'", tok.k == T_EOF ? "end of file" : tok.s);
    return NULL;
}
