/*
 * arinm2c: a Modula-2 (PIM4 subset) to C translator.
 *
 * First milestone: one MODULE with CONST/TYPE/VAR/PROCEDURE, the statements
 * IF WHILE REPEAT LOOP/EXIT FOR CASE RETURN, integers, BOOLEAN, CHAR,
 * POINTER, RECORD, ARRAY.  No module import, no REAL, no sets, no nested
 * procedures.  See status/modula2_fpc.md in daily-ops for the decisions
 * (integer widths, Z type for literals, VAL for width changes).
 *
 * One pass: the parser checks the types and writes the C text on the way.
 * The translator never frees memory (a one-shot process).
 *
 * WRITE and WRITELN are built in as a test aid, as in the FPC prototype.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------ utilities */

static const char *srcname;
static FILE *out;
static int ind;
static int errline;
static int quiet;      /* > 0: nothing is written (a definition module is only read) */

static void die(const char *f, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d: error: ", srcname, errline);
    va_start(ap, f);
    vfprintf(stderr, f, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static char *fmt(const char *f, ...)
{
    va_list ap, ap2;
    int n;
    char *p;
    va_start(ap, f);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, f, ap);
    va_end(ap);
    p = malloc((size_t)n + 1);
    if (!p)
        exit(1);
    vsnprintf(p, (size_t)n + 1, f, ap2);
    va_end(ap2);
    return p;
}

static void emit(const char *f, ...)
{
    va_list ap;
    int i;
    if (quiet)
        return;
    for (i = 0; i < ind; i++)
        fputs("  ", out);
    va_start(ap, f);
    vfprintf(out, f, ap);
    va_end(ap);
    fputc('\n', out);
}

/* the text of a C string literal for the bytes s */
static char *cstr(const char *s)
{
    size_t n = strlen(s) * 4 + 3, k = 0;
    char *p = malloc(n);
    p[k++] = '"';
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\')
            k += (size_t)sprintf(p + k, "\\%c", c);
        else if (c < 32 || c > 126)
            k += (size_t)sprintf(p + k, "\\%03o", c);
        else
            p[k++] = (char)c;
    }
    p[k++] = '"';
    p[k] = 0;
    return p;
}

static char *cchar(unsigned char c)
{
    if (c == '\'' || c == '\\')
        return fmt("'\\%c'", c);
    if (c < 32 || c > 126)
        return fmt("'\\%03o'", c);
    return fmt("'%c'", c);
}

/* ---------------------------------------------------------------- lexer */

enum { T_EOF, T_ID, T_KW, T_SYM, T_NUM, T_STR };

typedef struct {
    int k;
    char *s;
    long long v;
    int line;
} Tok;

static Tok tok;
static const char *src;
static size_t pos;
static int curline = 1;

static const char *kws[] = {
    "AND", "ARRAY", "BEGIN", "BY", "CASE", "CONST", "DEFINITION", "DIV", "DO",
    "ELSE", "ELSIF", "END", "EXIT", "EXPORT", "FOR", "FROM", "IF",
    "IMPLEMENTATION", "IMPORT", "IN", "LOOP", "MOD", "MODULE", "NOT", "OF",
    "OR", "POINTER", "PROCEDURE", "QUALIFIED", "RECORD", "REPEAT", "RETURN",
    "SET", "THEN", "TO", "TYPE", "UNTIL", "VAR", "WHILE", "WITH", 0};

static int iskw(const char *s)
{
    int i;
    for (i = 0; kws[i]; i++)
        if (!strcmp(kws[i], s))
            return 1;
    return 0;
}

static void next(void)
{
    size_t st;
    for (;;) {
        char c = src[pos];
        if (c == '\n') {
            curline++;
            pos++;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\f') {
            pos++;
        } else if (c == '(' && src[pos + 1] == '*') {
            int depth = 1;
            errline = curline;
            pos += 2;
            while (depth > 0) {
                if (!src[pos])
                    die("unterminated comment");
                if (src[pos] == '(' && src[pos + 1] == '*') {
                    depth++;
                    pos += 2;
                } else if (src[pos] == '*' && src[pos + 1] == ')') {
                    depth--;
                    pos += 2;
                } else {
                    if (src[pos] == '\n')
                        curline++;
                    pos++;
                }
            }
        } else
            break;
    }
    tok.line = errline = curline;
    tok.s = "";
    tok.v = 0;
    st = pos;
    if (!src[pos]) {
        tok.k = T_EOF;
    } else if (isalpha((unsigned char)src[pos])) {
        char *p;
        while (isalnum((unsigned char)src[pos]))
            pos++;
        p = malloc(pos - st + 1);
        memcpy(p, src + st, pos - st);
        p[pos - st] = 0;
        tok.s = p;
        tok.k = iskw(p) ? T_KW : T_ID;
    } else if (isdigit((unsigned char)src[pos])) {
        size_t n;
        char last, *p;
        while (isdigit((unsigned char)src[pos]) ||
               (src[pos] >= 'A' && src[pos] <= 'F'))
            pos++;
        n = pos - st;
        p = malloc(n + 2);
        memcpy(p, src + st, n);
        p[n] = 0;
        last = p[n - 1];
        if (src[pos] == 'H') {
            pos++;
            tok.v = strtoll(p, NULL, 16);
            tok.k = T_NUM;
        } else if (last == 'C' && n > 1) {
            p[n - 1] = 0;
            tok.v = strtoll(p, NULL, 8);
            tok.k = T_STR;
            tok.s = fmt("%c", (int)tok.v);
        } else {
            size_t i;
            for (i = 0; i < n; i++)
                if (!isdigit((unsigned char)p[i]))
                    die("bad number");
            tok.v = strtoll(p, NULL, 10);
            tok.k = T_NUM;
        }
    } else if (src[pos] == '\'' || src[pos] == '"') {
        char q = src[pos++];
        char *p;
        st = pos;
        while (src[pos] != q) {
            if (!src[pos] || src[pos] == '\n')
                die("unterminated string");
            pos++;
        }
        p = malloc(pos - st + 1);
        memcpy(p, src + st, pos - st);
        p[pos - st] = 0;
        pos++;
        tok.s = p;
        tok.k = T_STR;
    } else {
        static const char *two[] = {":=", "<=", ">=", "<>", "..", 0};
        int i;
        tok.k = T_SYM;
        for (i = 0; two[i]; i++)
            if (!strncmp(src + pos, two[i], 2)) {
                tok.s = (char *)two[i];
                pos += 2;
                return;
            }
        if (strchr("+-*/=#<>&~(),;:.[]{}^|", src[pos])) {
            tok.s = fmt("%c", src[pos]);
            pos++;
        } else {
            die("illegal character '%c'", src[pos]);
        }
    }
}

static int is(const char *s)
{
    return (tok.k == T_KW || tok.k == T_SYM) && !strcmp(tok.s, s);
}

static int accept(const char *s)
{
    if (is(s)) {
        next();
        return 1;
    }
    return 0;
}

static void expect(const char *s)
{
    if (!accept(s))
        die("'%s' expected, found '%s'", s,
            tok.k == T_EOF ? "end of file" : tok.s);
}

static char *ident(void)
{
    char *s;
    if (tok.k != T_ID)
        die("identifier expected, found '%s'",
            tok.k == T_EOF ? "end of file" : tok.s);
    s = tok.s;
    next();
    return s;
}

/* ---------------------------------------------------------------- types */

typedef enum { K_INT, K_BOOL, K_CHAR, K_PTR, K_REC, K_ARR, K_Z, K_STR,
               K_NIL, K_FWD } Kind;

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
};

static Type *mk(Kind k, int size, int sign, const char *cn)
{
    Type *t = calloc(1, sizeof *t);
    t->k = k;
    t->size = size;
    t->sign = sign;
    t->cname = (char *)cn;
    return t;
}

static Type *t_int8, *t_card8, *t_int16, *t_card16, *t_int32, *t_card32,
    *t_int64, *t_card64, *t_bool, *t_char, *t_z, *t_str, *t_nil;

static const char *tname(Type *t)
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

static int isint(Type *t) { return t->k == K_INT || t->k == K_Z; }

/* the type of an arithmetic expression: widths must agree (VAL changes them) */
static Type *arith(Type *a, Type *b)
{
    if (!isint(a) || !isint(b))
        die("integer operands expected (found %s and %s)", tname(a), tname(b));
    if (a->k == K_Z)
        return b;
    if (b->k == K_Z)
        return a;
    if (a != b)
        die("mixed integer types %s and %s: use VAL", tname(a), tname(b));
    return a;
}

/* ------------------------------------------------------------- symbols */

typedef struct Expr {
    char *c;            /* the C text */
    Type *t;
    int isconst;        /* an integer constant with the value v */
    long long v;
    int lvalue;
    int istype;         /* a type name (only as an argument of SIZEOF, VAL) */
    char *sraw;         /* the bytes of a string or CHAR constant */
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
    struct Module *next;
} Module;

static Sym *globals, *locals, *predef;
static int inproc;
static char *modname;
static Proc *curproc;
static int indef;       /* reading a definition module */
static int impl_mode;   /* translating an implementation module */
static int mainbody;    /* the module body is main (a program module) */
static Module *mods;

static Sym *lookup_in(Sym *l, const char *n)
{
    for (; l; l = l->next)
        if (!strcmp(l->name, n))
            return l;
    return NULL;
}

static Sym *find(const char *n)
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
static Sym *resolve(Sym *s)
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

static Sym *declare(const char *name, SKind k, Type *t)
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

static void init_predef(void)
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
}

/* --------------------------------------------------------- expressions */

static Expr expr(void);
static Type *type_(const char *hint);

static char *numc(long long v)
{
    if (v < 0)
        return v >= -2147483647LL ? fmt("(%lld)", v) : fmt("(%lldLL)", v);
    return v <= 2147483647LL ? fmt("%lld", v) : fmt("%lldLL", v);
}

static Expr constint(long long v)
{
    Expr e;
    memset(&e, 0, sizeof e);
    e.t = t_z;
    e.isconst = 1;
    e.v = v;
    e.c = numc(v);
    return e;
}

/* the left value or constant e can be assigned to a variable of type dst */
static void check_assign(Type *dst, Expr *e, const char *what)
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
        break;
    case K_BOOL: ok = s->k == K_BOOL; break;
    case K_CHAR: ok = s->k == K_CHAR; break;
    case K_PTR: ok = s->k == K_NIL || s == dst; break;
    case K_REC:
    case K_ARR: ok = s == dst; break;
    default: break;
    }
    if (!ok)
        die("cannot %s %s to %s", what, tname(s), tname(dst));
}

/* an expression of integer type converted to type t (C text) */
static char *conv(Type *dst, Expr *e)
{
    if (dst->k == K_INT && isint(e->t))
        return fmt("(%s)(%s)", dst->cname, e->c);
    return e->c;
}

static Expr call(Sym *s, int want_result)
{
    Expr e;
    Proc *p = s->proc;
    char *args = "";
    int i = 0;
    memset(&e, 0, sizeof e);
    if (want_result && !p->ret)
        die("'%s' is a procedure and has no value", s->name);
    if (!want_result && p->ret)
        die("the value of the function '%s' is not used", s->name);
    if (accept("(")) {
        if (!is(")")) {
            do {
                Expr a;
                if (i >= p->n)
                    die("too many arguments for '%s'", s->name);
                a = expr();
                if (p->p[i].isvar) {
                    if (!a.lvalue || a.t != p->p[i].t)
                        die("argument %d of '%s' must be a variable of type %s",
                            i + 1, s->name, tname(p->p[i].t));
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
        die("too few arguments for '%s'", s->name);
    e.t = p->ret;
    e.c = fmt("%s(%s)", s->cname, args);
    return e;
}

static Expr designator(Sym *s)
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
    e.c = s->isvarparam ? fmt("(*%s)", s->cname) : s->cname;
    e.t = s->t;
    e.lvalue = 1;
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
                if (e.t->k != K_ARR)
                    die("indexing of a type that is not an array");
                if (!isint(i.t))
                    die("an index must be an integer");
                e.c = fmt("%s.a[(int64_t)(%s)-(%lldLL)]", e.c, i.c, e.t->lo);
                e.t = e.t->base;
            } while (accept(","));
            expect("]");
        } else if (accept("^")) {
            if (e.t->k != K_PTR)
                die("'^' of a type that is not a pointer");
            if (e.t->base->k == K_FWD)
                die("the target type of the pointer is not declared");
            e.c = fmt("(*%s)", e.c);
            e.t = e.t->base;
        } else
            break;
    }
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
        Sym *s = resolve(find(n));
        if (s && s->k == S_PROC)
            return call(s, 1);
        if (s)
            return designator(s);
        /* built-in functions */
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
            if (!a.istype || a.t->k != K_INT)
                die("VAL: an integer type expected as the first argument");
            if (!isint(b.t) && b.t->k != K_CHAR && b.t->k != K_BOOL)
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
            if (a.t->k != K_CHAR && a.t->k != K_BOOL && a.t->k != K_INT)
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

static char *ctype_of(Type *t) { return t->k == K_Z ? "int64_t" : t->cname; }

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
            a = intop("+", a, b);
        } else if (accept("-")) {
            b = term();
            a = intop("-", a, b);
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

static Expr expr(void)
{
    Expr a = simple(), b, e;
    const char *op = NULL;
    if (is("=")) op = "==";
    else if (is("#") || is("<>")) op = "!=";
    else if (is("<")) op = "<";
    else if (is("<=")) op = "<=";
    else if (is(">")) op = ">";
    else if (is(">=")) op = ">=";
    if (!op)
        return a;
    next();
    b = simple();
    if (a.istype || b.istype)
        die("a type name cannot be used here");
    if (isint(a.t) && isint(b.t))
        arith(a.t, b.t);
    else if (a.t->k == K_CHAR && b.t->k == K_CHAR)
        ;
    else if (a.t->k == K_BOOL && b.t->k == K_BOOL &&
             (!strcmp(op, "==") || !strcmp(op, "!=")))
        ;
    else if ((a.t->k == K_PTR || a.t->k == K_NIL) &&
             (b.t->k == K_PTR || b.t->k == K_NIL) &&
             (a.t == b.t || a.t->k == K_NIL || b.t->k == K_NIL) &&
             (!strcmp(op, "==") || !strcmp(op, "!=")))
        ;
    else
        die("cannot compare %s with %s", tname(a.t), tname(b.t));
    memset(&e, 0, sizeof e);
    e.t = t_bool;
    e.c = fmt("(%s %s %s)", a.c, op, b.c);
    return e;
}

static Expr constexpr_(void)
{
    Expr e = expr();
    if (!e.isconst && !(e.t->k == K_CHAR && e.sraw) && !(e.t->k == K_BOOL))
        die("a constant expression is expected");
    return e;
}

/* ---------------------------------------------------------------- types */

static int tcount;

static char *autoname(void)
{
    return fmt("%s_%c%d", modname, indef ? 't' : 'l', ++tcount);
}

typedef struct Fwd {
    char *name;
    Type *t;
    struct Fwd *next;
} Fwd;
static Fwd *fwds;

static Type *record_type(const char *name)
{
    Type *t;
    Field **tail;
    Fwd *f, **fp;
    char *cn = name ? fmt("%s_%s", modname, name) : autoname();
    t = NULL;
    for (fp = &fwds; *fp; fp = &(*fp)->next)
        if (name && !strcmp((*fp)->name, name)) {
            t = (*fp)->t;
            *fp = (*fp)->next;
            break;
        }
    (void)f;
    if (!t) {
        t = mk(K_REC, 0, 0, cn);
        emit("typedef struct %s %s;", cn, cn);
    }
    t->k = K_REC;
    tail = &t->fields;
    emit("struct %s {", cn);
    ind++;
    while (!is("END")) {
        char *names[16];
        int n = 0, i;
        Type *ft;
        if (accept(";"))
            continue;
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
            *tail = fl;
            tail = &fl->next;
            emit("%s f_%s;", ft->cname, names[i]);
        }
    }
    expect("END");
    ind--;
    emit("};");
    if (!t->fields)
        die("an empty record is not supported");
    return t;
}

static Type *type_(const char *hint)
{
    Type *t;
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
        t = mk(K_PTR, 8, 0, cn);
        t->base = b;
        emit("typedef %s *%s;", b->cname, cn);
        return t;
    }
    if (accept("ARRAY")) {
        long long lo[8], hi[8];
        int n = 0, i;
        Type *el;
        char *cn;
        do {
            Expr a, b;
            expect("[");
            a = constexpr_();
            expect("..");
            b = constexpr_();
            expect("]");
            if (!a.isconst || !b.isconst)
                die("array bounds must be integer constants");
            if (n >= 8)
                die("too many array dimensions");
            lo[n] = a.v;
            hi[n] = b.v;
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

/* ----------------------------------------------------------- statements */

static void stmts(void);
static int loopid, curloop[32], nloop;

static void cond_block(void)
{
    ind++;
    stmts();
    ind--;
}

static Expr boolexpr(void)
{
    Expr c = expr();
    if (c.t->k != K_BOOL)
        die("a BOOLEAN expression is expected");
    return c;
}

static void write_args(int newline)
{
    if (accept("(")) {
        if (!is(")")) {
            do {
                Expr a = expr();
                Type *t = a.t;
                if (a.istype)
                    die("a type name cannot be written");
                if ((t->k == K_INT && t->sign) || t->k == K_Z)
                    emit("printf(\"%%lld\", (long long)(%s));", a.c);
                else if (t->k == K_INT)
                    emit("printf(\"%%llu\", (unsigned long long)(%s));", a.c);
                else if (t->k == K_CHAR)
                    emit("putchar(%s);", a.c);
                else if (t->k == K_STR)
                    emit("fputs(%s, stdout);", a.c);
                else if (t->k == K_BOOL)
                    emit("fputs((%s) ? \"TRUE\" : \"FALSE\", stdout);", a.c);
                else
                    die("cannot write a value of type %s", tname(t));
            } while (accept(","));
        }
        expect(")");
    }
    if (newline)
        emit("putchar('\\n');");
}

static void for_stmt(void)
{
    char *n = ident();
    Sym *s = resolve(find(n));
    Expr v, from, to;
    long long step = 1;
    int id = ++loopid;
    char *ct;
    if (!s || s->k != S_VAR)
        die("'%s' is not a variable", n);
    v = designator(s);
    if (v.t->k != K_INT)
        die("the FOR variable must be an integer variable");
    ct = v.t->cname;
    expect(":=");
    from = expr();
    check_assign(v.t, &from, "assign");
    expect("TO");
    to = expr();
    check_assign(v.t, &to, "assign");
    if (accept("BY")) {
        Expr b = constexpr_();
        if (!b.isconst)
            die("the BY step must be an integer constant");
        step = b.v;
        if (step == 0)
            die("the BY step is zero");
    }
    expect("DO");
    emit("{");
    ind++;
    emit("%s _lim%d = (%s)(%s);", ct, id, ct, to.c);
    emit("for (%s = (%s)(%s); %s %s _lim%d; %s += (%s)(%lld)) {", v.c, ct, from.c,
         v.c, step > 0 ? "<=" : ">=", id, v.c, ct, step);
    cond_block();
    emit("}");
    ind--;
    emit("}");
    expect("END");
}

static void case_stmt(void)
{
    Expr sel = expr();
    int id = ++loopid, had_else = 0;
    if (sel.t->k != K_INT && sel.t->k != K_CHAR && sel.t->k != K_Z)
        die("CASE needs an integer or CHAR selector");
    expect("OF");
    emit("{");
    ind++;
    emit("%s _sel%d = %s;", ctype_of(sel.t->k == K_Z ? t_int64 : sel.t), id,
         sel.c);
    emit("switch (_sel%d) {", id);
    ind++;
    for (;;) {
        if (accept("|"))
            continue;
        if (accept("ELSE")) {
            emit("default:");
            had_else = 1;
            cond_block();
            emit("  break;");
            break;
        }
        if (is("END"))
            break;
        do {
            Expr a = constexpr_(), b;
            long long lo, hi, k;
            if (a.t->k == K_CHAR) {
                lo = hi = (unsigned char)a.sraw[0];
            } else {
                if (!a.isconst)
                    die("a CASE label must be a constant");
                lo = hi = a.v;
            }
            if (accept("..")) {
                b = constexpr_();
                hi = b.t->k == K_CHAR ? (unsigned char)b.sraw[0] : b.v;
            }
            if (hi < lo || hi - lo > 1000)
                die("bad CASE label range");
            for (k = lo; k <= hi; k++) {
                if (sel.t->k == K_CHAR)
                    emit("case %s:", cchar((unsigned char)k));
                else
                    emit("case %lld:", k);
            }
        } while (accept(","));
        expect(":");
        cond_block();
        emit("  break;");
    }
    if (!had_else) {
        emit("default:");
        emit("  abort();");
    }
    ind--;
    emit("}");
    ind--;
    emit("}");
    expect("END");
}

static void stmt(void)
{
    if (accept("IF")) {
        Expr c = boolexpr();
        expect("THEN");
        emit("if (%s) {", c.c);
        cond_block();
        while (accept("ELSIF")) {
            c = boolexpr();
            expect("THEN");
            emit("} else if (%s) {", c.c);
            cond_block();
        }
        if (accept("ELSE")) {
            emit("} else {");
            cond_block();
        }
        emit("}");
        expect("END");
    } else if (accept("WHILE")) {
        Expr c = boolexpr();
        expect("DO");
        emit("while (%s) {", c.c);
        cond_block();
        emit("}");
        expect("END");
    } else if (accept("REPEAT")) {
        Expr c;
        emit("do {");
        cond_block();
        expect("UNTIL");
        c = boolexpr();
        emit("} while (!%s);", c.c);
    } else if (accept("LOOP")) {
        int id = ++loopid;
        if (nloop >= 32)
            die("loops nested too deeply");
        curloop[nloop++] = id;
        emit("for (;;) {");
        cond_block();
        emit("}");
        emit("L_exit%d:;", id);
        nloop--;
        expect("END");
    } else if (accept("EXIT")) {
        if (!nloop)
            die("EXIT outside of a LOOP");
        emit("goto L_exit%d;", curloop[nloop - 1]);
    } else if (accept("FOR")) {
        for_stmt();
    } else if (accept("CASE")) {
        case_stmt();
    } else if (accept("RETURN")) {
        if (curproc && curproc->ret) {
            Expr e = expr();
            check_assign(curproc->ret, &e, "return");
            emit("return %s;", conv(curproc->ret, &e));
        } else {
            if (!is(";") && !is("END") && !is("ELSE") && !is("ELSIF"))
                die("a procedure cannot return a value");
            emit(curproc || !mainbody ? "return;" : "return 0;");
        }
    } else if (tok.k == T_ID) {
        char *n = ident();
        Sym *s = resolve(find(n));
        if (s && s->k == S_PROC) {
            Expr c = call(s, 0);
            emit("%s;", c.c);
        } else if (s && s->k == S_VAR) {
            Expr l = designator(s), r;
            expect(":=");
            r = expr();
            if (!l.lvalue)
                die("cannot assign to this");
            check_assign(l.t, &r, "assign");
            emit("%s = %s;", l.c, conv(l.t, &r));
        } else if (s) {
            die("'%s' cannot be used as a statement", n);
        } else if (!strcmp(n, "WRITE")) {
            write_args(0);
        } else if (!strcmp(n, "WRITELN")) {
            write_args(1);
        } else if (!strcmp(n, "NEW") || !strcmp(n, "DISPOSE")) {
            Expr a;
            expect("(");
            a = expr();
            expect(")");
            if (!a.lvalue || a.t->k != K_PTR)
                die("%s needs a pointer variable", n);
            if (n[0] == 'N')
                emit("%s = calloc(1, sizeof(*%s));", a.c, a.c);
            else
                emit("free(%s);", a.c);
        } else if (!strcmp(n, "INC") || !strcmp(n, "DEC")) {
            Expr a, k;
            int inc = n[0] == 'I';
            expect("(");
            a = expr();
            if (accept(","))
                k = expr();
            else
                k = constint(1);
            expect(")");
            if (!a.lvalue || a.t->k != K_INT)
                die("%s needs an integer variable", n);
            check_assign(a.t, &k, "add");
            emit("%s %s= (%s)(%s);", a.c, inc ? "+" : "-", a.t->cname, k.c);
        } else if (!strcmp(n, "HALT")) {
            emit("exit(1);");
        } else {
            die("'%s' is not declared", n);
        }
    }
    /* otherwise: the empty statement */
}

static void stmts(void)
{
    for (;;) {
        stmt();
        if (!accept(";"))
            break;
    }
}

/* ---------------------------------------------------------- declarations */

static void const_decl(void)
{
    while (tok.k == T_ID) {
        char *n = ident();
        Expr e;
        Sym *s;
        expect("=");
        e = constexpr_();
        s = declare(n, S_CONST, e.t);
        s->ce = e;
        expect(";");
    }
}

static void type_decl(void)
{
    while (tok.k == T_ID) {
        char *n = ident();
        Type *t;
        Sym *s;
        expect("=");
        t = type_(n);
        if (!t->cname)
            die("this type cannot be named");
        s = declare(n, S_TYPE, t);
        (void)s;
        expect(";");
    }
}

static void var_decl(void)
{
    while (tok.k == T_ID) {
        char *names[32];
        int n = 0, i;
        Type *t;
        do {
            if (n >= 32)
                die("too many variables in one declaration");
            names[n++] = ident();
        } while (accept(","));
        expect(":");
        t = type_(NULL);
        for (i = 0; i < n; i++) {
            Sym *s = declare(names[i], S_VAR, t);
            if (indef)
                emit("extern %s %s;", t->cname, s->cname);
            else
                emit("%s%s %s;", inproc ? "" : "static ", t->cname, s->cname);
        }
        expect(";");
    }
}

static void check_fwds(void)
{
    if (fwds)
        die("the pointer target type '%s' is never declared", fwds->name);
}

static void decls(void);

static void proc_decl(void)
{
    char *n = ident();
    Proc *p = calloc(1, sizeof *p);
    Sym *s;
    char *params = NULL, *proto;
    int i, reuse = 0;
    Proc *old = NULL;
    if (inproc)
        die("nested procedures are not supported yet");
    s = NULL;
    if (impl_mode && !indef) {
        s = lookup_in(globals, n);
        if (s && !(s->k == S_PROC && s->exported && !s->defined))
            s = NULL;
    }
    if (s) {
        reuse = 1;
        old = s->proc;
        s->proc = p;
    } else {
        s = declare(n, S_PROC, NULL);
        s->proc = p;
    }
    if (accept("(")) {
        if (!is(")")) {
            do {
                int isvar = accept("VAR");
                char *names[16];
                int k = 0, j;
                Type *t;
                do {
                    if (k >= 16)
                        die("too many parameters");
                    names[k++] = ident();
                } while (accept(","));
                expect(":");
                t = type_(NULL);
                for (j = 0; j < k; j++) {
                    if (p->n >= 16)
                        die("too many parameters");
                    p->p[p->n].name = names[j];
                    p->p[p->n].t = t;
                    p->p[p->n].isvar = isvar;
                    p->n++;
                }
            } while (accept(";"));
        }
        expect(")");
        if (accept(":"))
            p->ret = type_(NULL);
    }
    expect(";");
    if (reuse) {
        int bad = old->n != p->n || old->ret != p->ret;
        for (i = 0; !bad && i < p->n; i++)
            bad = old->p[i].t != p->p[i].t || old->p[i].isvar != p->p[i].isvar;
        if (bad)
            die("procedure '%s' does not match the definition module", n);
        s->defined = 1;
    }
    for (i = 0; i < p->n; i++)
        params = fmt("%s%s%s %sv_%s", params ? params : "", i ? ", " : "",
                     p->p[i].t->cname, p->p[i].isvar ? "*" : "", p->p[i].name);
    proto = fmt("%s%s %s(%s)", (reuse || indef) ? "" : "static ",
                p->ret ? p->ret->cname : "void", s->cname,
                params ? params : "void");
    if (indef) {
        emit("%s;", proto);
        return;
    }
    if (!reuse)
        emit("%s;", proto);
    emit("%s {", proto);
    ind++;
    inproc = 1;
    locals = NULL;
    curproc = p;
    for (i = 0; i < p->n; i++) {
        Sym *ps = declare(p->p[i].name, S_VAR, p->p[i].t);
        ps->isvarparam = p->p[i].isvar;
    }
    decls();
    if (accept("BEGIN"))
        stmts();
    expect("END");
    if (strcmp(ident(), n))
        die("the procedure end name does not match '%s'", n);
    expect(";");
    ind--;
    emit("}");
    inproc = 0;
    locals = NULL;
    curproc = NULL;
}

static void decls(void)
{
    for (;;) {
        if (accept("CONST"))
            const_decl();
        else if (accept("TYPE"))
            type_decl();
        else if (accept("VAR"))
            var_decl();
        else if (accept("PROCEDURE"))
            proc_decl();
        else
            break;
        if (!inproc)
            check_fwds();
    }
}

/* --------------------------------------------------------------- modules */

static char *incdirs[16];
static int nincdirs;
static Module *initlist[32];
static int ninit;
static char *included[64];
static int nincluded;

static char *readfile(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *buf;
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)n + 1);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n)
        exit(1);
    buf[n] = 0;
    fclose(f);
    return buf;
}

/* the path of name+ext in the source directory or in a -I directory */
static char *findfile(const char *name, const char *ext)
{
    int i;
    for (i = 0; i < nincdirs; i++) {
        char *p = fmt("%s/%s%s", incdirs[i], name, ext);
        FILE *f = fopen(p, "rb");
        if (f) {
            fclose(f);
            return p;
        }
    }
    return NULL;
}

static void def_module(const char *expected, int header);
static void import_list(void);

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

static void save(Saved *v)
{
    v->src = src; v->srcname = srcname; v->pos = pos; v->curline = curline;
    v->errline = errline; v->inproc = inproc; v->tcount = tcount;
    v->quiet = quiet; v->indef = indef; v->impl_mode = impl_mode;
    v->mainbody = mainbody; v->tok = tok; v->globals = globals;
    v->locals = locals; v->modname = modname; v->curproc = curproc;
    v->fwds = fwds;
}

static void restore(Saved *v)
{
    src = v->src; srcname = v->srcname; pos = v->pos; curline = v->curline;
    errline = v->errline; inproc = v->inproc; tcount = v->tcount;
    quiet = v->quiet; indef = v->indef; impl_mode = v->impl_mode;
    mainbody = v->mainbody; tok = v->tok; globals = v->globals;
    locals = v->locals; modname = v->modname; curproc = v->curproc;
    fwds = v->fwds;
}

/* the definition module name is read (once) and its declarations are known */
static Module *load_module(const char *name)
{
    Module *m;
    Saved sv;
    char *path, *text;
    for (m = mods; m; m = m->next)
        if (!strcmp(m->name, name))
            return m;
    path = findfile(name, ".def");
    if (!path)
        die("the definition module %s was not found (%s.def)", name, name);
    text = readfile(path);
    save(&sv);
    src = text;
    srcname = path;
    pos = 0;
    curline = 1;
    globals = locals = NULL;
    inproc = 0;
    curproc = NULL;
    fwds = NULL;
    tcount = 0;
    quiet++;
    impl_mode = 0;
    mainbody = 0;
    next();
    m = calloc(1, sizeof *m);
    m->name = (char *)name;
    m->hasimpl = findfile(name, ".mod") != NULL;
    def_module(name, 0);
    m->syms = globals;
    m->next = mods;
    mods = m;
    restore(&sv);
    return m;
}

static void use_module(Module *md)
{
    int i;
    if (quiet)
        return;
    for (i = 0; i < nincluded; i++)
        if (!strcmp(included[i], md->name))
            return;
    if (nincluded >= 64)
        die("too many imported modules");
    included[nincluded++] = md->name;
    emit("#include \"%s.h\"", md->name);
    if (!indef && md->hasimpl) {
        if (ninit >= 32)
            die("too many imported modules");
        initlist[ninit++] = md;
    }
}

/* FROM m IMPORT x: the name x is known without the module name */
static void alias(Sym *s)
{
    Sym *c;
    if (lookup_in(globals, s->name))
        die("'%s' is declared twice", s->name);
    c = calloc(1, sizeof *c);
    *c = *s;
    c->imported = 1;
    c->exported = 0;
    c->next = globals;
    globals = c;
}

static void import_list(void)
{
    while (is("IMPORT") || is("FROM")) {
        if (accept("FROM")) {
            Module *md = load_module(ident());
            expect("IMPORT");
            do {
                char *n = ident();
                Sym *s;
                for (s = md->syms; s; s = s->next)
                    if (!s->imported && !strcmp(s->name, n))
                        break;
                if (!s)
                    die("module %s has no '%s'", md->name, n);
                alias(s);
            } while (accept(","));
            expect(";");
            use_module(md);
        } else {
            expect("IMPORT");
            do {
                Sym *s;
                char *n = ident();
                Module *md = load_module(n);
                if (lookup_in(globals, n))
                    die("'%s' is declared twice", n);
                s = calloc(1, sizeof *s);
                s->name = n;
                s->k = S_MODULE;
                s->mod = md;
                s->next = globals;
                globals = s;
                use_module(md);
            } while (accept(","));
            expect(";");
        }
    }
}

static void std_includes(void)
{
    emit("#include <stdint.h>");
    emit("#include <stdio.h>");
    emit("#include <stdlib.h>");
}

static void def_module(const char *expected, int header)
{
    char *name;
    expect("DEFINITION");
    expect("MODULE");
    name = ident();
    if (expected && strcmp(expected, name))
        die("the file defines module %s, not %s", name, expected);
    expect(";");
    modname = name;
    indef = 1;
    if (header) {
        emit("#ifndef %s_H", name);
        emit("#define %s_H", name);
        emit("#include <stdint.h>");
    }
    import_list();
    decls();
    expect("END");
    if (strcmp(ident(), name))
        die("the definition module end name does not match '%s'", name);
    expect(".");
    if (header) {
        if (findfile(name, ".mod"))
            emit("void %s_init(void);", name);
        emit("#endif");
    }
    indef = 0;
}

static void body_start(void)
{
    int i;
    for (i = 0; i < ninit; i++)
        emit("%s_init();", initlist[i]->name);
}

/* IMPLEMENTATION MODULE X: the declarations of X.def are the module's own */
static void impl_module(void)
{
    Module *m;
    Sym *s, *c;
    expect("IMPLEMENTATION");
    expect("MODULE");
    modname = ident();
    expect(";");
    m = load_module(modname);
    impl_mode = 1;
    globals = NULL;
    for (s = m->syms; s; s = s->next) {
        if (s->imported)
            continue;
        c = calloc(1, sizeof *c);
        *c = *s;
        c->next = globals;
        globals = c;
    }
    std_includes();
    emit("#include \"%s.h\"", modname);
    import_list();
    for (s = globals; s; s = s->next)
        if (s->k == S_VAR && s->exported)
            emit("%s %s;", s->t->cname, s->cname);
    decls();
    emit("");
    emit("void %s_init(void)", modname);
    emit("{");
    ind++;
    emit("static int done;");
    emit("if (done)");
    emit("  return;");
    emit("done = 1;");
    body_start();
    if (accept("BEGIN"))
        stmts();
    expect("END");
    if (strcmp(ident(), modname))
        die("the module end name does not match '%s'", modname);
    expect(".");
    ind--;
    emit("}");
    for (s = globals; s; s = s->next)
        if (s->k == S_PROC && s->exported && !s->defined)
            die("procedure '%s' of the definition module has no body", s->name);
    if (tok.k != T_EOF)
        die("text after the end of the module");
}

static void module(void)
{
    expect("MODULE");
    modname = ident();
    expect(";");
    mainbody = 1;
    std_includes();
    import_list();
    decls();
    emit("");
    emit("int main(void)");
    emit("{");
    ind++;
    body_start();
    if (accept("BEGIN"))
        stmts();
    expect("END");
    if (strcmp(ident(), modname))
        die("the module end name does not match '%s'", modname);
    expect(".");
    emit("return 0;");
    ind--;
    emit("}");
    if (tok.k != T_EOF)
        die("text after the end of the module");
}

int main(int argc, char **argv)
{
    char *text, *slash;
    const char *outname = NULL;
    int i;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc)
            outname = argv[++i];
        else if (!strcmp(argv[i], "-I") && i + 1 < argc) {
            if (nincdirs >= 15) {
                fprintf(stderr, "too many -I\n");
                return 2;
            }
            incdirs[++nincdirs] = argv[++i];
        } else if (!srcname)
            srcname = argv[i];
        else {
            srcname = NULL;
            break;
        }
    }
    if (!srcname) {
        fprintf(stderr, "usage: arinm2c [-I dir] file.{mod,def} [-o file]\n");
        return 2;
    }
    text = readfile(srcname);
    if (!text) {
        perror(srcname);
        return 1;
    }
    incdirs[0] = fmt("%s", srcname);
    slash = strrchr(incdirs[0], '/');
    if (slash)
        *slash = 0;
    else
        incdirs[0] = ".";
    nincdirs++;
    src = text;
    out = stdout;
    if (outname && !(out = fopen(outname, "w"))) {
        perror(outname);
        return 1;
    }
    init_predef();
    next();
    if (is("DEFINITION"))
        def_module(NULL, 1);
    else if (is("IMPLEMENTATION"))
        impl_module();
    else
        module();
    if (tok.k != T_EOF)
        die("text after the end of the module");
    if (outname)
        fclose(out);
    return 0;
}
