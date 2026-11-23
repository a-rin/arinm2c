/* decl: declarations: CONST TYPE VAR PROCEDURE and local modules */
#include "arinm2c.h"

/* the declaration (C text) of a local variable of a procedure, in the order of
   the declarations: a local of the C function, or a field of the frame */
typedef struct LV {
    char *decl;
    struct LV *next;
} LV;

/* a procedure that is being read */
typedef struct PCtx {
    Sym *ps;
    Proc *p;
    int framed;         /* it has nested procedures: its variables are in a frame */
    LV *vars, **vtail;
    struct PCtx *parent;
} PCtx;
static PCtx *cur_p;

/* The C text of a procedure is kept until the outermost procedure is done: C
   has no nested functions, so the function of a nested procedure goes before
   the one of the procedure around it. */
typedef struct Done {
    char *proto;        /* the prototype, or NULL */
    char *fwd;          /* the forward declaration of the struct of the frame */
    char *frame;        /* the struct of the frame */
    char *text;         /* the function */
} Done;
static Done done[64];
static int ndone;

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
        if (indef && is(";")) {
            /* an opaque type: C sees a pointer to an unknown struct */
            Type *ot = mk(K_PTR, 8, 0, fmt("%s_%s", modname, n));
            ot->copaque = 1;
            emit("typedef struct %s_s *%s;", ot->cname, ot->cname);
            declare(n, S_TYPE, ot);
            expect(";");
            continue;
        }
        if (impl_mode && !inproc) {
            /* the implementation module completes an opaque type */
            Sym *o = lookup_in(globals, n);
            if (o && o->k == S_TYPE && o->exported && o->t->copaque &&
                !o->t->base) {
                expect("=");
                if (!is("POINTER"))
                    die("the opaque type '%s' must be completed by a pointer "
                        "type (POINTER TO ...); a subrange is not supported", n);
                expect("POINTER");
                expect("TO");
                o->t->base = pointer_target();
                expect(";");
                continue;
            }
        }
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
        if (t->k == K_OPEN)
            die("an open array can only be a parameter");
        for (i = 0; i < n; i++) {
            Sym *s = declare(names[i], S_VAR, t);
            if (indef) {
                emit("extern %s %s;", t->cname, s->cname);
            } else if (inproc) {
                LV *lv = calloc(1, sizeof *lv);
                lv->decl = fmt("%s %s;", t->cname, s->cname);
                *cur_p->vtail = lv;
                cur_p->vtail = &lv->next;
            } else {
                emit("static %s %s;", t->cname, s->cname);
            }
        }
        expect(";");
    }
}

static void check_fwds(void)
{
    if (fwds)
        die("the pointer target type '%s' is never declared", fwds->name);
}

/* the fields and the first lines of a procedure that has a frame, or the
   local variables of a procedure that has none, for the parameters */
static void param_code(PCtx *c, int framed, char **fields, char **init)
{
    Proc *p = c->p;
    int i, k;
    *fields = "";
    *init = "";
    for (i = 0; i < p->n; i++) {
        Param *q = &p->p[i];
        char *v = fmt("v_%s", q->name);
        if (q->t->k == K_OPEN) {
            int nd = open_dims(q->t);
            char *cnt = "1", *src = q->isvar ? v : fmt("%s_in", v);
            for (k = 0; k < nd; k++)
                cnt = fmt("%s * (%s + 1)", cnt, high_name(v, k));
            if (framed) {
                *fields = fmt("%s  %s%s;\n", *fields, q->t->cname, v);
                for (k = 0; k < nd; k++)
                    *fields = fmt("%s  int64_t %s;\n", *fields, high_name(v, k));
                if (q->isvar) {
                    *init = fmt("%s  _fr.%s = %s;\n", *init, v, src);
                } else {
                    *init = fmt("%s  %s %s_cp[%s];\n  memcpy(%s_cp, %s, sizeof(%s_cp));\n"
                                "  _fr.%s = %s_cp;\n",
                                *init, open_leaf(q->t)->cname, v, cnt, v, src, v,
                                v, v);
                }
                for (k = 0; k < nd; k++)
                    *init = fmt("%s  _fr.%s = %s;\n", *init, high_name(v, k),
                                high_name(v, k));
            } else if (!q->isvar) {
                *init = fmt("%s  %s %s[%s];\n  memcpy(%s, %s, sizeof(%s));\n", *init,
                            open_leaf(q->t)->cname, v, cnt, v, src, v);
            }
        } else if (framed) {
            *fields = fmt("%s  %s %s%s;\n", *fields, q->t->cname,
                          q->isvar ? "*" : "", v);
            *init = fmt("%s  _fr.%s = %s;\n", *init, v, v);
        }
    }
}

/* the text of the outermost procedure and of the ones nested in it */
static void flush_procs(void)
{
    int i;
    if (types_text) {
        fputs(types_text, out);
        types_text = NULL;
    }
    for (i = 0; i < ndone; i++)
        if (done[i].fwd)
            fputs(done[i].fwd, out);
    for (i = 0; i < ndone; i++)
        if (done[i].frame)
            fputs(done[i].frame, out);
    for (i = 0; i < ndone; i++)
        if (done[i].proto)
            fputs(done[i].proto, out);
    for (i = 0; i < ndone; i++)
        fputs(done[i].text, out);
    ndone = 0;
}

static void proc_decl(void)
{
    char *n = ident();
    Proc *p = calloc(1, sizeof *p);
    Sym *s, *sy;
    char *params = NULL, *proto, *outer_mod = modname, *body = "", *text;
    char *fields, *init, *fr;
    int i, reuse = 0, nested = inproc, saved_ind = ind;
    Proc *old = NULL;
    PCtx ctx, *parent = cur_p;
    Done *d;
    /* ponytail: seven levels (lstack) and 60 procedures per outermost
       procedure (done); upgrade: grow the arrays */
    if (nested && (nl >= 7 || ndone >= 60))
        die("procedures nested too deeply");
    s = NULL;
    if (impl_mode && !indef && !inproc) {
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
    if (nested)
        s->cname = fmt("%s_%s", modname, n);
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
    for (i = 0; i < p->n; i++) {
        char *one;
        if (p->p[i].t->k == K_OPEN)
            one = open_params(p->p[i].t, p->p[i].name, p->p[i].isvar);
        else
            one = fmt("%s %sv_%s", p->p[i].t->cname, p->p[i].isvar ? "*" : "",
                      p->p[i].name);
        params = fmt("%s%s%s", params ? params : "", i ? ", " : "", one);
    }
    if (nested)
        params = fmt("struct Fr_%s *_up%s%s", parent->ps->cname,
                     params ? ", " : "", params ? params : "");
    proto = fmt("%s%s %s(%s)", (reuse || indef) ? "" : "static ",
                p->ret ? p->ret->cname : "void", s->cname,
                params ? params : "void");
    if (indef) {
        emit("%s;", proto);
        return;
    }

    /* a procedure inside a procedure: the outer one gets a frame */
    if (nested && !parent->framed) {
        parent->framed = 1;
        for (sy = locals; sy; sy = sy->next)
            if (sy->k == S_VAR)
                sy->framed = 1;
    }
    memset(&ctx, 0, sizeof ctx);
    ctx.ps = s;
    ctx.p = p;
    ctx.vtail = &ctx.vars;
    ctx.parent = parent;
    cur_p = &ctx;
    if (nested)
        lstack[nl++] = locals;
    modname = fmt("%s_%s", outer_mod, n);
    ind = 1;
    hoist_types++;
    inproc = 1;
    plevel++;
    locals = NULL;
    curproc = p;
    cur_framed = 0;
    for (i = 0; i < p->n; i++) {
        Sym *ps = declare(p->p[i].name, S_VAR, p->p[i].t);
        ps->isvarparam = p->p[i].isvar;
    }
    decls();
    if (accept("BEGIN")) {
        FILE *o = cap_begin();
        stmts();
        body = cap_end(o);
    }
    expect("END");
    if (strcmp(ident(), n))
        die("the procedure end name does not match '%s'", n);
    expect(";");

    /* the C function */
    fr = fmt("Fr_%s", s->cname);
    param_code(&ctx, ctx.framed, &fields, &init);
    text = fmt("%s {\n", proto);
    d = &done[ndone];
    memset(d, 0, sizeof *d);
    if (ctx.framed) {
        LV *lv;
        char *fl = nested ? fmt("  struct Fr_%s *_up;\n", parent->ps->cname) : "";
        fl = fmt("%s%s", fl, fields);
        for (lv = ctx.vars; lv; lv = lv->next)
            fl = fmt("%s  %s\n", fl, lv->decl);
        d->fwd = fmt("struct %s;\n", fr);
        d->frame = fmt("struct %s {\n%s};\n", fr, fl);
        text = fmt("%s  struct %s _fr;\n%s%s", text, fr,
                   nested ? "  _fr._up = _up;\n" : "", init);
    } else {
        LV *lv;
        for (lv = ctx.vars; lv; lv = lv->next)
            text = fmt("%s  %s\n", text, lv->decl);
        text = fmt("%s%s", text, init);
    }
    d->text = fmt("%s%s}\n\n", text, body);
    d->proto = reuse ? NULL : fmt("%s;\n", proto);
    ndone++;

    ind = saved_ind;
    hoist_types--;
    modname = outer_mod;
    plevel--;
    inproc = nested;
    locals = nested ? lstack[--nl] : NULL;
    curproc = parent ? parent->p : NULL;
    cur_p = parent;
    cur_framed = parent ? parent->framed : 0;
    if (!nested)
        flush_procs();
}

char *lmod_inits[16];

int nlmod;

/* MODULE m; IMPORT ...; EXPORT ...; declarations BEGIN statements END m;
   The names of m are in a scope of their own; only the exported ones are
   known outside. The body runs at the start of the enclosing module. */
static void local_module(void)
{
    char *name, *outer_mod = modname;
    Sym *saved = globals, *inner, *sy, *ex[32];
    char *exports[32];
    int nexp = 0, qualified = 0, i;
    if (inproc || indef)
        die("a local module is allowed at the level of a module only");
    name = ident();
    expect(";");
    globals = NULL;
    modname = fmt("%s_%s", outer_mod, name);
    while (is("IMPORT") || is("FROM")) {
        if (accept("FROM")) {
            char *m = ident();
            Sym *ms = lookup_in(saved, m);
            Module *md = (ms && ms->k == S_MODULE) ? ms->mod : load_module(m);
            expect("IMPORT");
            do {
                char *n = ident();
                for (sy = md->syms; sy; sy = sy->next)
                    if (!sy->imported && !strcmp(sy->name, n))
                        break;
                if (!sy)
                    die("module %s has no '%s'", md->name, n);
                if (!already_aliased(sy))
                    alias(sy);
                alias_enum_consts(sy, md->syms);
            } while (accept(","));
            expect(";");
            if (!ms)
                use_module(md);
        } else {
            expect("IMPORT");
            do {
                char *n = ident();
                Sym *o = lookup_in(saved, n);
                if (o) {
                    alias(o);
                } else {
                    Module *md = load_module(n);
                    sy = calloc(1, sizeof *sy);
                    sy->name = n;
                    sy->k = S_MODULE;
                    sy->mod = md;
                    sy->next = globals;
                    globals = sy;
                    use_module(md);
                }
            } while (accept(","));
            expect(";");
        }
    }
    if (accept("EXPORT")) {
        qualified = accept("QUALIFIED");
        do {
            if (nexp >= 32)
                die("too many exported names");
            exports[nexp++] = ident();
        } while (accept(","));
        expect(";");
    }
    decls();
    if (accept("BEGIN")) {
        int savedmain = mainbody;
        char *init = fmt("%s_init", modname);
        if (nlmod >= 16)
            die("too many local modules");
        mainbody = 0;
        emit("");
        emit("static void %s(void)", init);
        emit("{");
        ind++;
        stmts();
        ind--;
        emit("}");
        mainbody = savedmain;
        lmod_inits[nlmod++] = init;
    }
    expect("END");
    if (strcmp(ident(), name))
        die("the module end name does not match '%s'", name);
    expect(";");
    inner = globals;
    for (i = 0; i < nexp; i++) {
        sy = lookup_in(inner, exports[i]);
        if (!sy)
            die("'%s' is exported by %s but not declared", exports[i], name);
        ex[i] = sy;
    }
    globals = saved;
    modname = outer_mod;
    if (qualified) {
        Module *md = calloc(1, sizeof *md);
        md->name = name;
        for (i = 0; i < nexp; i++) {
            Sym *c = calloc(1, sizeof *c), *k;
            *c = *ex[i];
            c->imported = 0;
            c->next = md->syms;
            md->syms = c;
            /* the constants of an exported enumeration type */
            if (ex[i]->k == S_TYPE && ex[i]->t->k == K_ENUM && !ex[i]->t->sub)
                for (k = inner; k; k = k->next)
                    if (k->k == S_CONST && k->t == ex[i]->t) {
                        Sym *cc = calloc(1, sizeof *cc);
                        *cc = *k;
                        cc->imported = 0;
                        cc->next = md->syms;
                        md->syms = cc;
                    }
        }
        if (lookup_in(globals, name))
            die("'%s' is declared twice", name);
        sy = calloc(1, sizeof *sy);
        sy->name = name;
        sy->k = S_MODULE;
        sy->mod = md;
        sy->next = globals;
        globals = sy;
    } else {
        for (i = 0; i < nexp; i++) {
            if (!already_aliased(ex[i]))
                alias(ex[i]);
            alias_enum_consts(ex[i], inner);
        }
    }
}

void decls(void)
{
    for (;;) {
        if (accept("MODULE"))
            local_module();
        else if (accept("CONST"))
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
