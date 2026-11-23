/* expr: type checking and C text of expressions, designators and calls */
#include "arinm2c.h"

char *numc(long long v)
{
    if (v < 0)
        return v >= -2147483647LL ? fmt("(%lld)", v) : fmt("(%lldLL)", v);
    return v <= 2147483647LL ? fmt("%lld", v) : fmt("%lldLL", v);
}

Expr constint(long long v)
{
    Expr e;
    memset(&e, 0, sizeof e);
    e.t = t_z;
    e.isconst = 1;
    e.v = v;
    e.c = numc(v);
    return e;
}

static int sig_equal(Proc *a, Proc *b)
{
    int i;
    if (a->n != b->n || a->ret != b->ret)
        return 0;
    for (i = 0; i < a->n; i++)
        if (a->p[i].t != b->p[i].t || a->p[i].isvar != b->p[i].isvar)
            return 0;
    return 1;
}

static int isset(Type *t) { return t->k == K_SET || t->k == K_SETLIT; }

/* the value of a constant: an integer, an enumeration constant, a character */
long long cval(Expr *e)
{
    if (e->t->k == K_CHAR && e->sraw)
        return (unsigned char)e->sraw[0];
    return e->v;
}

/* the left value or constant e can be assigned to a variable of type dst */
void check_assign(Type *dst, Expr *e, const char *what)
{
    Type *s = e->t;
    int ok = 0;
    if (e->istype)
        die("a type name cannot be used here");
    switch (dst->k) {
    case K_INT:
        ok = s->k == K_Z || (s->k == K_INT && s->size == dst->size);
        if (s->k == K_INT && !ok)
            die("cannot %s %s to %s: use VAL", what, tname(s), tname(dst));
        if (ok && dst->sub && e->isconst && (e->v < dst->slo || e->v > dst->shi))
            die("the constant %lld is outside the range %lld..%lld", e->v,
                dst->slo, dst->shi);
        break;
    case K_BOOL: ok = s->k == K_BOOL; break;
    case K_CHAR: ok = s->k == K_CHAR; break;
    case K_ENUM: ok = s->k == K_ENUM && H(s) == H(dst); break;
    case K_PTR:
        ok = s->k == K_NIL || s == dst ||
             (s->k == K_PTR && (dst->isaddr || s->isaddr));
        break;
    case K_SET: ok = s->k == K_SETLIT || s == dst; break;
    case K_PROC:
        ok = s->k == K_NIL ||
             (s->k == K_PROC && (s == dst || sig_equal(s->sig, dst->sig)));
        break;
    case K_REC:
    case K_ARR: ok = s == dst; break;
    default: break;
    }
    if (!ok)
        die("cannot %s %s to %s", what, tname(s), tname(dst));
}

/* an expression of integer type converted to type t (C text) */
char *conv(Type *dst, Expr *e)
{
    if (dst->k == K_INT && isint(e->t))
        return fmt("(%s)(%s)", dst->cname, e->c);
    return e->c;
}

/* the C arguments (a pointer and the high bound) for an open array parameter */
static char *open_arg(Type *pt, Expr *a, const char *who)
{
    Type *el = pt->base;
    int n = open_dims(pt), k;
    if (n > 1) {
        /* ARRAY OF ARRAY OF T: the elements are contiguous */
        Type *leaf = open_leaf(pt);
        char *r;
        if (a->t->k == K_OPEN && open_dims(a->t) == n && open_leaf(a->t) == leaf) {
            r = fmt("%s", a->c);
            for (k = 0; k < n; k++)
                r = fmt("%s, %s", r, a->highs[k]);
            return r;
        } else {
            Type *x = a->t;
            r = fmt("(%s)&%s", pt->cname, a->c);
            for (k = 0; k < n; k++) {
                if (x->k != K_ARR)
                    die("the argument of '%s' does not fit the open array "
                        "parameter", who);
                r = fmt("%s, %lld", r, x->hi - x->lo);
                x = x->base;
            }
            if (x != leaf || !a->lvalue)
                die("the argument of '%s' does not fit the open array "
                    "parameter", who);
            return r;
        }
    }
    if (el == t_byte) {
        if (a->t->k == K_OPEN)
            return fmt("(uint8_t *)%s, ((%s) + 1) * (int64_t)sizeof(*%s) - 1",
                       a->c, a->highs[0], a->c);
        if (!a->lvalue)
            die("an argument of '%s' for ARRAY OF BYTE must be a variable", who);
        return fmt("(uint8_t *)&%s, (int64_t)sizeof(%s) - 1", a->c, a->c);
    }
    if (a->t->k == K_STR || (a->t->k == K_CHAR && a->sraw)) {
        if (el != t_char)
            die("a string cannot be passed to '%s'", who);
        return fmt("%s, %d", cstr(a->sraw), (int)strlen(a->sraw) - 1);
    }
    if (a->t->k == K_ARR && a->t->base == el)
        return fmt("%s.a, %lld", a->c, a->t->hi - a->t->lo);
    if (a->t->k == K_OPEN && a->t->base == el)
        return fmt("%s, %s", a->c, a->highs[0]);
    die("the argument of '%s' does not fit the open array parameter", who);
}

Expr call_with(Proc *p, const char *cfunc, const char *name,
                      int want_result)
{
    Expr e;
    char *args = "";
    int i = 0;
    memset(&e, 0, sizeof e);
    if (want_result && !p->ret)
        die("'%s' is a procedure and has no value", name);
    if (!want_result && p->ret)
        die("the value of the function '%s' is not used", name);
    if (accept("(")) {
        if (!is(")")) {
            do {
                Expr a;
                if (i >= p->n)
                    die("too many arguments for '%s'", name);
                a = expr();
                if (p->p[i].t->k == K_OPEN) {
                    args = fmt("%s%s%s", args, i ? ", " : "",
                               open_arg(p->p[i].t, &a, name));
                } else if (p->p[i].isvar) {
                    if (!a.lvalue || a.t != p->p[i].t)
                        die("argument %d of '%s' must be a variable of type %s",
                            i + 1, name, tname(p->p[i].t));
                    args = fmt("%s%s&%s", args, i ? ", " : "", a.c);
                } else {
                    check_assign(p->p[i].t, &a, "pass");
                    args = fmt("%s%s%s", args, i ? ", " : "",
                               conv(p->p[i].t, &a));
                }
                i++;
            } while (accept(","));
        }
        expect(")");
    }
    if (i != p->n)
        die("too few arguments for '%s'", name);
    e.t = p->ret;
    e.c = fmt("%s(%s)", cfunc, args);
    return e;
}

Expr call(Sym *s, int want_result)
{
    if (!s->proc)
        die("'%s' cannot be used here", s->name);
    return call_with(s->proc, s->cname, s->name, want_result);
}

/* a procedure name used as a value (assigned to a procedure variable) */
static Expr procvalue(Sym *s)
{
    Expr e;
    memset(&e, 0, sizeof e);
    if (!s->t) {
        s->t = mk(K_PROC, 8, 0, NULL);
        s->t->sig = s->proc;
    }
    e.t = s->t;
    e.c = s->cname;
    return e;
}

With with_stack[16];

int nwith;

int with_field(const char *n, Expr *e)
{
    int i;
    for (i = nwith - 1; i >= 0; i--) {
        Field *f;
        for (f = with_stack[i].rec->fields; f; f = f->next)
            if (!strcmp(f->name, n)) {
                memset(e, 0, sizeof *e);
                e->c = fmt("%s.f_%s", with_stack[i].c, n);
                e->t = f->t;
                e->lvalue = 1;
                return 1;
            }
    }
    return 0;
}

Expr designator(Sym *s)
{
    Expr e;
    memset(&e, 0, sizeof e);
    if (s->k == S_CONST)
        return s->ce;
    if (s->k == S_TYPE) {
        e.istype = 1;
        e.t = s->t;
        e.c = s->t->cname;
        return e;
    }
    e.c = (s->isvarparam && s->t->k != K_OPEN) ? fmt("(*%s)", s->cname)
                                               : s->cname;
    e.t = s->t;
    e.lvalue = 1;
    if (s->t->k == K_OPEN) {
        int k;
        for (k = 0; k < open_dims(s->t); k++)
            e.highs[k] = high_name(s->cname, k);
    }
    return designator_tail(e);
}

Expr designator_tail(Expr e)
{
    for (;;) {
        if (accept(".")) {
            Field *f;
            char *n = ident();
            if (e.t->k != K_REC)
                die("'.%s': not a record", n);
            for (f = e.t->fields; f && strcmp(f->name, n); f = f->next)
                ;
            if (!f)
                die("the record has no field '%s'", n);
            e.c = fmt("%s.f_%s", e.c, n);
            e.t = f->t;
        } else if (accept("[")) {
            do {
                Expr i = expr();
                if (e.t->k == K_OPEN) {
                    int k;
                    if (!isint(i.t))
                        die("an index must be an integer");
                    if (e.t->base->k == K_OPEN) {
                        char *stride = "1";
                        for (k = 1; e.highs[k]; k++)
                            stride = fmt("%s * ((%s) + 1)", stride, e.highs[k]);
                        e.c = fmt("(%s + (int64_t)(%s) * %s)", e.c, i.c, stride);
                        for (k = 0; k < 3; k++)
                            e.highs[k] = e.highs[k + 1];
                        e.highs[3] = NULL;
                    } else {
                        e.c = fmt("%s[(int64_t)(%s)]", e.c, i.c);
                        e.highs[0] = NULL;
                    }
                    e.t = e.t->base;
                    continue;
                }
                if (e.t->k != K_ARR)
                    die("indexing of a type that is not an array");
                if (!isint(i.t) && i.t->k != K_ENUM && i.t->k != K_CHAR)
                    die("an index must be an ordinal value");
                e.c = fmt("%s.a[(int64_t)(%s)-(%lldLL)]", e.c, i.c, e.t->lo);
                e.t = e.t->base;
            } while (accept(","));
            expect("]");
        } else if (accept("^")) {
            if (e.t->k != K_PTR)
                die("'^' of a type that is not a pointer");
            if (!e.t->base)
                die("'^' of an ADDRESS or of an opaque type");
            if (e.t->base->k == K_FWD)
                die("the target type of the pointer is not declared");
            if (e.t->copaque)
                e.c = fmt("(*(%s *)%s)", e.t->base->cname, e.c);
            else
                e.c = fmt("(*%s)", e.c);
            e.t = e.t->base;
        } else
            break;
    }
    return e;
}

/* a procedure variable followed by arguments is a call */
static Expr finish_call(Expr e, const char *name)
{
    if (e.t->k == K_PROC && is("("))
        return call_with(e.t->sig, e.c, name, 1);
    return e;
}

static Expr set_constructor(void)
{
    Expr e;
    char *c = NULL;
    memset(&e, 0, sizeof e);
    if (!is("}")) {
        do {
            Expr a = expr(), b;
            char *part;
            if (!isint(a.t) && a.t->k != K_ENUM && a.t->k != K_CHAR)
                die("a set element must be an ordinal value");
            if (accept("..")) {
                b = expr();
                part = fmt("m2_range(%s, %s)", a.c, b.c);
            } else
                part = fmt("m2_bit(%s)", a.c);
            c = c ? fmt("%s | %s", c, part) : part;
        } while (accept(","));
    }
    expect("}");
    e.t = t_setlit;
    e.c = c ? fmt("(%s)", c) : "((uint64_t)0)";
    return e;
}

static Expr factor(void)
{
    Expr e;
    memset(&e, 0, sizeof e);
    if (tok.k == T_NUM) {
        e = constint(tok.v);
        next();
        return e;
    }
    if (tok.k == T_STR) {
        e.sraw = tok.s;
        if (strlen(tok.s) == 1) {
            e.t = t_char;
            e.c = cchar((unsigned char)tok.s[0]);
        } else {
            e.t = t_str;
            e.c = cstr(tok.s);
        }
        next();
        return e;
    }
    if (accept("{"))
        return set_constructor();
    if (accept("(")) {
        e = expr();
        expect(")");
        if (!e.isconst)
            e.c = fmt("(%s)", e.c);
        e.lvalue = 0;
        return e;
    }
    if (accept("NOT") || accept("~")) {
        e = factor();
        if (e.t->k != K_BOOL)
            die("NOT needs a BOOLEAN");
        e.c = fmt("(!%s)", e.c);
        e.lvalue = 0;
        return e;
    }
    if (tok.k == T_ID) {
        char *n = ident();
        Expr w;
        Sym *s;
        if (with_field(n, &w))
            return finish_call(designator_tail(w), n);
        s = resolve(find(n));
        if (s && s->k == S_PROC && !s->proc) {
            Expr a;
            expect("(");
            a = expr();
            expect(")");
            if (!a.lvalue)
                die("ADR needs a variable");
            e.t = t_addr;
            e.c = fmt("((void *)&%s)", a.c);
            return e;
        }
        if (s && s->k == S_PROC) {
            if (is("("))
                return call(s, 1);
            return procvalue(s);
        }
        if (s && s->k == S_TYPE && s->t->k == K_SET && is("{")) {
            /* Digits{1, 2} */
            next();
            e = set_constructor();
            e.t = s->t;
            return e;
        }
        if (s)
            return finish_call(designator(s), n);
        /* built-in functions */
        if (!strcmp(n, "HIGH")) {
            Expr a;
            expect("(");
            a = expr();
            expect(")");
            if (a.t->k == K_OPEN) {
                e.t = t_z;
                e.c = fmt("((int64_t)(%s))", a.highs[0]);
                return e;
            }
            if (a.t->k == K_ARR)
                return constint(a.t->hi);
            if (a.t->k == K_STR)
                return constint((long long)strlen(a.sraw) - 1);
            die("HIGH needs an array");
        }
        if (!strcmp(n, "SIZEOF")) {
            Expr a;
            expect("(");
            a = expr();
            expect(")");
            e.t = t_z;
            e.c = fmt("((int64_t)sizeof(%s))", a.istype ? a.t->cname : a.c);
            if (!a.istype && a.t->k == K_STR)
                die("SIZEOF of a string constant");
            return e;
        }
        if (!strcmp(n, "VAL")) {
            Expr a, b;
            expect("(");
            a = expr();
            expect(",");
            b = expr();
            expect(")");
            if (a.istype && a.t->isaddr && b.t->k == K_INT && b.t->size == 8 &&
                !b.t->sign) {
                e.t = a.t;
                e.c = fmt("((void *)(uintptr_t)(%s))", b.c);
                return e;
            }
            if (a.istype && a.t->k == K_INT && a.t->size == 8 && !a.t->sign &&
                b.t->isaddr) {
                e.t = a.t;
                e.c = fmt("((uint64_t)(uintptr_t)(%s))", b.c);
                return e;
            }
            if (!a.istype || (a.t->k != K_INT && a.t->k != K_ENUM &&
                              a.t->k != K_CHAR))
                die("VAL: an ordinal type expected as the first argument");
            if (!isint(b.t) && b.t->k != K_CHAR && b.t->k != K_BOOL &&
                b.t->k != K_ENUM)
                die("VAL: an ordinal value expected");
            e.t = a.t;
            e.c = fmt("((%s)(%s))", a.t->cname, b.c);
            return e;
        }
        if (!strcmp(n, "ORD")) {
            Expr a;
            expect("(");
            a = expr();
            expect(")");
            if (a.t->k != K_CHAR && a.t->k != K_BOOL && a.t->k != K_INT &&
                a.t->k != K_ENUM)
                die("ORD: an ordinal value expected");
            e.t = t_z;
            e.c = fmt("((int64_t)(%s))", a.c);
            return e;
        }
        if (!strcmp(n, "CHR")) {
            Expr a;
            expect("(");
            a = expr();
            expect(")");
            if (!isint(a.t))
                die("CHR: an integer expected");
            e.t = t_char;
            e.c = fmt("((char)(%s))", a.c);
            return e;
        }
        if (!strcmp(n, "ODD")) {
            Expr a;
            expect("(");
            a = expr();
            expect(")");
            if (!isint(a.t))
                die("ODD: an integer expected");
            e.t = t_bool;
            e.c = fmt("(((%s) & 1) != 0)", a.c);
            return e;
        }
        if (!strcmp(n, "ABS")) {
            Expr a;
            expect("(");
            a = expr();
            expect(")");
            if (!isint(a.t) || (a.t->k == K_INT && !a.t->sign))
                die("ABS: a signed integer expected");
            e.t = a.t;
            e.c = fmt("((%s)((%s) < 0 ? -(%s) : (%s)))", a.t->cname, a.c, a.c,
                      a.c);
            return e;
        }
        die("'%s' is not declared", n);
    }
    die("expression expected, found '%s'",
        tok.k == T_EOF ? "end of file" : tok.s);
    return e;
}

char *ctype_of(Type *t) { return t->k == K_Z ? "int64_t" : t->cname; }

static Expr intop(const char *op, Expr a, Expr b)
{
    Expr e;
    Type *rt = arith(a.t, b.t);
    memset(&e, 0, sizeof e);
    e.t = rt;
    if (a.isconst && b.isconst) {
        long long x = a.v, y = b.v, r = 0;
        if (!strcmp(op, "+")) r = x + y;
        else if (!strcmp(op, "-")) r = x - y;
        else if (!strcmp(op, "*")) r = x * y;
        else {
            if (y == 0)
                die("division by zero in a constant");
            r = !strcmp(op, "/") ? x / y : x % y;
        }
        return constint(r);
    }
    e.c = fmt("((%s)(%s %s %s))", ctype_of(rt), a.c, op, b.c);
    return e;
}

static Expr setop(const char *op, Expr a, Expr b)
{
    Expr e;
    const char *co = !strcmp(op, "+") ? "|" : !strcmp(op, "-") ? "& ~" : "&";
    memset(&e, 0, sizeof e);
    if (!isset(a.t) || !isset(b.t))
        die("a set operator needs two sets");
    if (a.t->k == K_SET && b.t->k == K_SET && a.t != b.t)
        die("sets of different types");
    e.t = a.t->k == K_SET ? a.t : b.t;
    e.c = fmt("(%s %s %s)", a.c, co, b.c);
    return e;
}

static Expr addop(const char *op, Expr a, Expr b)
{
    if (isset(a.t) || isset(b.t))
        return setop(op, a, b);
    return intop(op, a, b);
}

static Expr term(void)
{
    Expr a = factor();
    for (;;) {
        Expr b;
        const char *op;
        if (accept("*"))
            op = "*";
        else if (accept("DIV"))
            op = "/";
        else if (accept("MOD"))
            op = "%";
        else if (accept("AND") || accept("&")) {
            b = factor();
            if (a.t->k != K_BOOL || b.t->k != K_BOOL)
                die("AND needs BOOLEAN operands");
            a.c = fmt("(%s && %s)", a.c, b.c);
            a.isconst = 0;
            a.lvalue = 0;
            continue;
        } else if (is("/"))
            die("'/' needs REAL, which is not supported yet");
        else
            return a;
        b = factor();
        if (isset(a.t) || isset(b.t)) {
            if (strcmp(op, "*"))
                die("this operator is not defined for sets");
            a = setop(op, a, b);
        } else
            a = intop(op, a, b);
    }
}

static Expr simple(void)
{
    Expr a;
    int neg = 0;
    if (accept("-"))
        neg = 1;
    else
        accept("+");
    a = term();
    if (neg) {
        if (!isint(a.t) || (a.t->k == K_INT && !a.t->sign))
            die("unary minus needs a signed integer");
        if (a.isconst)
            a = constint(-a.v);
        else
            a.c = fmt("((%s)(-%s))", ctype_of(a.t), a.c);
        a.lvalue = 0;
    }
    for (;;) {
        Expr b;
        if (accept("+")) {
            b = term();
            a = addop("+", a, b);
        } else if (accept("-")) {
            b = term();
            a = addop("-", a, b);
        } else if (accept("OR")) {
            b = term();
            if (a.t->k != K_BOOL || b.t->k != K_BOOL)
                die("OR needs BOOLEAN operands");
            a.c = fmt("(%s || %s)", a.c, b.c);
            a.isconst = 0;
            a.lvalue = 0;
        } else
            return a;
    }
}

Expr expr(void)
{
    Expr a = simple(), b, e;
    const char *op = NULL;
    if (is("=")) op = "==";
    else if (is("#") || is("<>")) op = "!=";
    else if (is("<")) op = "<";
    else if (is("<=")) op = "<=";
    else if (is(">")) op = ">";
    else if (is(">=")) op = ">=";
    else if (is("IN")) op = "IN";
    if (!op)
        return a;
    next();
    b = simple();
    if (a.istype || b.istype)
        die("a type name cannot be used here");
    memset(&e, 0, sizeof e);
    e.t = t_bool;
    if (!strcmp(op, "IN")) {
        if (!isset(b.t) || (!isint(a.t) && a.t->k != K_ENUM && a.t->k != K_CHAR))
            die("IN needs an ordinal value and a set");
        e.c = fmt("(((%s) >> (%s)) & 1)", b.c, a.c);
        return e;
    }
    if (isset(a.t) || isset(b.t)) {
        if (!isset(a.t) || !isset(b.t) ||
            (a.t->k == K_SET && b.t->k == K_SET && a.t != b.t))
            die("cannot compare %s with %s", tname(a.t), tname(b.t));
        if (!strcmp(op, "==") || !strcmp(op, "!="))
            e.c = fmt("(%s %s %s)", a.c, op, b.c);
        else if (!strcmp(op, "<="))
            e.c = fmt("((%s & ~%s) == 0)", a.c, b.c);
        else if (!strcmp(op, ">="))
            e.c = fmt("((%s & ~%s) == 0)", b.c, a.c);
        else
            die("this comparison is not defined for sets");
        return e;
    }
    if (isint(a.t) && isint(b.t))
        arith(a.t, b.t);
    else if (a.t->k == K_CHAR && b.t->k == K_CHAR)
        ;
    else if (a.t->k == K_ENUM && b.t->k == K_ENUM && H(a.t) == H(b.t))
        ;
    else if (a.t->k == K_BOOL && b.t->k == K_BOOL &&
             (!strcmp(op, "==") || !strcmp(op, "!=")))
        ;
    else if ((a.t->k == K_PTR || a.t->k == K_NIL) &&
             (b.t->k == K_PTR || b.t->k == K_NIL) &&
             (a.t == b.t || a.t->k == K_NIL || b.t->k == K_NIL ||
              a.t->isaddr || b.t->isaddr) &&
             (!strcmp(op, "==") || !strcmp(op, "!=")))
        ;
    else
        die("cannot compare %s with %s", tname(a.t), tname(b.t));
    memset(&e, 0, sizeof e);
    e.t = t_bool;
    e.c = fmt("(%s %s %s)", a.c, op, b.c);
    return e;
}

Expr constexpr_(void)
{
    Expr e = expr();
    if (!e.isconst && !(e.t->k == K_CHAR && e.sraw) && e.t->k != K_BOOL &&
        e.t->k != K_STR)
        die("a constant expression is expected");
    return e;
}
