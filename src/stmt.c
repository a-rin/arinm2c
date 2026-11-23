/* stmt: statements */
#include "arinm2c.h"

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
                else if (t->k == K_REAL || t->k == K_RZ)
                    emit("printf(\"%%g\", (double)(%s));", a.c);
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
    if (v.t->k != K_INT && v.t->k != K_ENUM && v.t->k != K_CHAR)
        die("the FOR variable must be an ordinal variable");
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
    if (sel.t->k != K_INT && sel.t->k != K_CHAR && sel.t->k != K_Z &&
        sel.t->k != K_ENUM)
        die("CASE needs an ordinal selector");
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
            if (a.t->k != K_CHAR && !a.isconst)
                die("a CASE label must be a constant");
            lo = hi = cval(&a);
            if (accept("..")) {
                b = constexpr_();
                hi = cval(&b);
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

static void with_stmt(void)
{
    char *n = ident();
    Expr w, d;
    Sym *sy = NULL;
    int id = ++loopid;
    if (with_field(n, &w))
        d = designator_tail(w);
    else {
        sy = resolve(find(n));
        if (!sy || sy->k != S_VAR)
            die("'%s' is not a variable", n);
        d = designator(sy);
    }
    if (d.t->k != K_REC || !d.lvalue)
        die("WITH needs a record variable");
    expect("DO");
    if (nwith >= 16)
        die("WITH nested too deeply");
    emit("{");
    ind++;
    emit("%s *_w%d = &%s;", d.t->cname, id, d.c);
    with_stack[nwith].rec = d.t;
    with_stack[nwith].c = fmt("(*_w%d)", id);
    nwith++;
    cond_block();
    nwith--;
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
    } else if (accept("WITH")) {
        with_stmt();
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
        Expr w;
        Sym *s = NULL;
        int wf = with_field(n, &w);
        if (!wf)
            s = resolve(find(n));
        if (wf || (s && s->k == S_VAR)) {
            Expr l = wf ? designator_tail(w) : designator(s), r;
            if (l.t->k == K_PROC && !is(":=")) {
                Expr c = call_with(l.t->sig, l.c, n, 0, NULL);
                emit("%s;", c.c);
            } else {
                expect(":=");
                r = expr();
                if (!l.lvalue)
                    die("cannot assign to this");
                check_assign(l.t, &r, "assign");
                emit("%s = %s;", l.c, conv(l.t, &r));
            }
        } else if (s && s->k == S_PROC) {
            Expr c = call(s, 0);
            emit("%s;", c.c);
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
            if (!a.lvalue || a.t->k != K_PTR || !a.t->base)
                die("%s needs a pointer variable with a type (not an ADDRESS or "
                    "an opaque type)", n);
            if (n[0] == 'N' && a.t->copaque)
                emit("%s = calloc(1, sizeof(%s));", a.c, a.t->base->cname);
            else if (n[0] == 'N')
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
            if (!a.lvalue || (a.t->k != K_INT && a.t->k != K_ENUM &&
                              a.t->k != K_CHAR))
                die("%s needs an ordinal variable", n);
            if (a.t->k == K_INT)
                check_assign(a.t, &k, "add");
            else if (!isint(k.t))
                die("%s: an integer is expected", n);
            emit("%s %s= (%s)(%s);", a.c, inc ? "+" : "-", a.t->cname, k.c);
        } else if (!strcmp(n, "INCL") || !strcmp(n, "EXCL")) {
            Expr a, k;
            expect("(");
            a = expr();
            expect(",");
            k = expr();
            expect(")");
            if (!a.lvalue || a.t->k != K_SET)
                die("%s needs a set variable", n);
            if (n[0] == 'I')
                emit("%s |= m2_bit(%s);", a.c, k.c);
            else
                emit("%s &= ~m2_bit(%s);", a.c, k.c);
        } else if (!strcmp(n, "HALT")) {
            emit("exit(1);");
        } else {
            die("'%s' is not declared", n);
        }
    }
    /* otherwise: the empty statement */
}

void stmts(void)
{
    for (;;) {
        stmt();
        if (!accept(";"))
            break;
    }
}
