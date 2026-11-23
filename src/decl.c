/* decl: declarations: CONST TYPE VAR PROCEDURE and local modules */
#include "arinm2c.h"

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
    for (i = 0; i < p->n; i++) {
        char *one;
        if (p->p[i].t->k == K_OPEN)
            one = fmt("%sv_%s%s, int64_t v_%s_high", p->p[i].t->cname,
                      p->p[i].name, p->p[i].isvar ? "" : "_in", p->p[i].name);
        else
            one = fmt("%s %sv_%s", p->p[i].t->cname, p->p[i].isvar ? "*" : "",
                      p->p[i].name);
        params = fmt("%s%s%s", params ? params : "", i ? ", " : "", one);
    }
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
        if (p->p[i].t->k == K_OPEN && !p->p[i].isvar) {
            /* a value parameter is a copy */
            char *el = p->p[i].t->base->cname;
            emit("%s v_%s[v_%s_high + 1];", el, p->p[i].name, p->p[i].name);
            emit("memcpy(v_%s, v_%s_in, sizeof(v_%s));", p->p[i].name,
                 p->p[i].name, p->p[i].name);
        }
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
                alias(sy);
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
            Sym *c = calloc(1, sizeof *c);
            *c = *ex[i];
            c->imported = 0;
            c->next = md->syms;
            md->syms = c;
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
        for (i = 0; i < nexp; i++)
            alias(ex[i]);
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
