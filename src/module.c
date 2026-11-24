/* module: definition, implementation and program modules; IMPORT */
#include "arinm2c.h"
#include "md5.h"

char **incdirs;

int nincdirs;

static Module **initlist;

static int ninit;

static char **included;

static int nincluded;

char *readfile(const char *path)
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
char *findfile(const char *name, const char *ext)
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

static void import_list(void);

int already_aliased(Sym *s)
{
    Sym *o = lookup_in(globals, s->name);
    return o && o->imported && o->k == S_CONST && s->k == S_CONST && o->t == s->t;
}

void alias_enum_consts(Sym *ts, Sym *list)
{
    Sym *c;
    if (ts->k != S_TYPE || ts->t->k != K_ENUM || ts->t->sub)
        return;
    for (c = list; c; c = c->next)
        if (c->k == S_CONST && c->t == ts->t && !c->imported &&
            !lookup_in(globals, c->name))
            alias(c);
}

/* the name of a module is the name of its file, with the same case */
static void check_filename(const char *name)
{
    const char *b = strrchr(srcname, '/');
    size_t n = strlen(name);
    b = b ? b + 1 : srcname;
    if (strncmp(b, name, n) != 0 || b[n] != '.')
        die("the module name '%s' does not match the file name '%s'", name, b);
}

/* the version of a definition module: the MD5 of its tokens, so that a change
   of a comment or of the layout is not a new version (the tokens are
   separated by a newline: a changed name or number is a new version) */
void def_md5(const char *text, char out[33])
{
    const char *osrc = src;
    size_t opos = pos, n = 0, cap = 4096;
    int ocur = curline, oerr = errline;
    Tok otok = tok;
    char *buf = malloc(cap);
    src = text;
    pos = 0;
    curline = 1;
    for (next(); tok.k != T_EOF; next()) {
        char *t = fmt("%d %s %lld\n", tok.k, tok.s, tok.v);
        size_t k = strlen(t);
        while (n + k > cap)
            buf = realloc(buf, cap *= 2);
        memcpy(buf + n, t, k);
        n += k;
    }
    md5_hex(buf, n, out);
    free(buf);
    src = osrc;
    pos = opos;
    curline = ocur;
    errline = oerr;
    tok = otok;
}

/* the header of a module that has code: every file that includes it refers to
   the symbol X__def_<md5>, only the implementation defines it (the module
   defines M2_IMPLEMENTS_X before it includes the header), so the link of
   objects made with two different versions of the definition module fails */
static void version_check(const char *name, const char *md5)
{
    emit("#define %s_DEF_MD5 \"%s\"", name, md5);
    if (!findfile(name, ".mod") && !findfile(name, ".c"))
        return;
    emit("#ifdef M2_IMPLEMENTS_%s", name);
    emit("const char %s__def_%s = 0;", name, md5);
    emit("#else");
    emit("extern const char %s__def_%s;", name, md5);
    emit("static const char *const %s__use_%s __attribute__((used)) = &%s__def_%s;",
         name, md5, name, md5);
    emit("#endif");
}

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
Module *load_module(const char *name)
{
    Module *m;
    Saved sv;
    char *path, *text, md5[33];
    static char *loading[16];
    static int nloading;
    int i;
    for (m = mods; m; m = m->next)
        if (!strcmp(m->name, name))
            return m;
    path = findfile(name, ".def");
    if (!path)
        die("the definition module %s was not found (%s.def)", name, name);
    for (i = 0; i < nloading; i++)
        if (!strcmp(loading[i], name))
            die("circular import of the module %s", name);
    if (nloading >= 16)
        die("modules imported too deeply");
    loading[nloading++] = fmt("%s", name);
    text = readfile(path);
    save(&sv);
    def_md5(text, md5);
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
    memcpy(m->md5, md5, sizeof md5);
    def_module(name, 0);
    m->syms = globals;
    m->next = mods;
    mods = m;
    nloading--;
    restore(&sv);
    return m;
}

void use_module(Module *md)
{
    int i;
    if (quiet || md->builtin)
        return;
    for (i = 0; i < nincluded; i++)
        if (!strcmp(included[i], md->name))
            return;
    included = grow(included, nincluded, sizeof *included);
    included[nincluded++] = md->name;
    emit("#include \"%s.h\"", md->name);
    if (!indef && md->hasimpl) {
        initlist = grow(initlist, ninit, sizeof *initlist);
        initlist[ninit++] = md;
    }
}

/* FROM m IMPORT x: the name x is known without the module name */
void alias(Sym *s)
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
                if (!already_aliased(s))
                    alias(s);
                alias_enum_consts(s, md->syms);
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
    emit("#include <stdarg.h>");
    emit("#include <stdlib.h>");
    emit("#include <string.h>");
    emit("#define M2_UNUSED __attribute__((unused))");
    emit("/* compile with -fwrapv: a signed overflow wraps.");
    emit("   -DM2_NOCHECK: no check of the indices and of the divisors.");
    emit("   -DM2_ABORT: a run time error calls abort (a core dump) and not exit(1). */");
    emit("static void m2_fail(const char *file, int line, const char *what, ...)");
    emit("    __attribute__((noreturn, unused));");
    emit("static void m2_fail(const char *file, int line, const char *what, ...)");
    emit("{");
    emit("  va_list ap;");
    emit("  fprintf(stderr, \"%%s:%%d: \", file, line);");
    emit("  va_start(ap, what);");
    emit("  vfprintf(stderr, what, ap);");
    emit("  va_end(ap);");
    emit("  fputc('\\n', stderr);");
    emit("#ifdef M2_ABORT");
    emit("  abort();");
    emit("#else");
    emit("  exit(1);");
    emit("#endif");
    emit("}");
    emit("#ifdef M2_NOCHECK");
    emit("#define m2_idx(i, lo, hi, file, line) ((int64_t)(i) - (lo))");
    emit("#define m2_chk(b, file, line) (b)");
    emit("#else");
    emit("M2_UNUSED static inline int64_t m2_idx(int64_t i, int64_t lo, int64_t hi,");
    emit("                             const char *file, int line)");
    emit("{");
    emit("  if (i < lo || i > hi)");
    emit("    m2_fail(file, line, \"index %%lld is outside %%lld..%%lld\", (long long)i,");
    emit("            (long long)lo, (long long)hi);");
    emit("  return i - lo;");
    emit("}");
    emit("M2_UNUSED static inline int64_t m2_chk(int64_t b, const char *file, int line)");
    emit("{");
    emit("  if (b == 0)");
    emit("    m2_fail(file, line, \"division by zero\");");
    emit("  return b;");
    emit("}");
    emit("#endif");
    emit("M2_UNUSED static inline void *m2_alloc(size_t n, const char *file, int line)");
    emit("{");
    emit("  void *p = calloc(1, n);");
    emit("  if (!p)");
    emit("    m2_fail(file, line, \"out of memory\");");
    emit("  return p;");
    emit("}");
    emit("M2_UNUSED static inline uint64_t m2_bit(int64_t i) { return (uint64_t)1 << i; }");
    emit("M2_UNUSED static inline uint64_t m2_range(int64_t a, int64_t b)");
    emit("{");
    emit("  uint64_t r = 0;");
    emit("  for (; a <= b; a++)");
    emit("    r |= (uint64_t)1 << a;");
    emit("  return r;");
    emit("}");
}

void def_module(const char *expected, int header)
{
    char *name;
    expect("DEFINITION");
    expect("MODULE");
    name = ident();
    check_filename(name);
    if (expected && strcmp(expected, name))
        die("the file defines module %s, not %s", name, expected);
    expect(";");
    modname = name;
    indef = 1;
    if (header) {
        emit("#ifndef %s_H", name);
        emit("#define %s_H", name);
        emit("#include <stdint.h>");
        {
            char md5[33];
            def_md5(src, md5);
            version_check(name, md5);
        }
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

/* the versions of all the definition modules that were read (the imported
   ones and what they import) as comments, for arinm2c -V */
static void emit_versions(void)
{
    Module *m;
    for (m = mods; m; m = m->next)
        if (m->md5[0])
            emit("/* m2def %s %s */", m->name, m->md5);
}

static void body_start(void)
{
    int i;
    for (i = 0; i < ninit; i++)
        emit("%s_init();", initlist[i]->name);
    for (i = 0; i < nlmod; i++)
        emit("%s();", lmod_inits[i]);
}

/* IMPLEMENTATION MODULE X: the declarations of X.def are the module's own */
void impl_module(void)
{
    Module *m;
    Sym *s, *c;
    expect("IMPLEMENTATION");
    expect("MODULE");
    modname = ident();
    check_filename(modname);
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
    emit("#define M2_IMPLEMENTS_%s", modname);
    emit("#include \"%s.h\"", modname);
    import_list();
    for (s = globals; s; s = s->next)
        if (s->k == S_VAR && s->exported)
            emit("%s %s;", s->t->cname, s->cname);
    decls();
    for (s = globals; s; s = s->next)
        if (s->k == S_TYPE && s->exported && s->t->copaque && !s->t->base)
            die("the opaque type '%s' is not completed", s->name);
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
    emit_versions();
}

void module(void)
{
    expect("MODULE");
    modname = ident();
    check_filename(modname);
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
    emit_versions();
}
