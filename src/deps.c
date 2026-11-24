/* deps: arinm2c -M Main.mod writes a Makefile that builds the program */
#include "arinm2c.h"

/* A module as the Makefile sees it: its files and the modules that they
   import. The imports are found by a scan of the tokens (IMPORT and FROM), the
   source is not translated. */
typedef struct DM {
    char *name;
    char *defpath, *modpath, *cpath;    /* the files that exist, or NULL */
    char *defimps[32], *modimps[32];    /* the modules imported by .def, .mod */
    int ndi, nmi;
    int visiting, linked;
    struct DM *next;
} DM;

static DM *dms;

static void add_import(char **list, int *n, const char *name)
{
    int i;
    if (!strcmp(name, "SYSTEM") || !findfile(name, ".def"))
        return;         /* a name that is not a module */
    for (i = 0; i < *n; i++)
        if (!strcmp(list[i], name))
            return;
    if (*n >= 32)
        die("%s imports too many modules", srcname);
    list[(*n)++] = fmt("%s", name);
}

/* the modules named by FROM m IMPORT and IMPORT m in a source file */
static void scan_imports(const char *path, char **list, int *n)
{
    const char *osrc = src, *osrcname = srcname;
    size_t opos = pos;
    int ocur = curline, oerr = errline;
    Tok otok = tok;
    char *text = readfile(path);
    if (!text) {
        perror(path);
        exit(1);
    }
    src = text;
    srcname = path;
    pos = 0;
    curline = 1;
    next();
    while (tok.k != T_EOF) {
        if (is("FROM")) {
            next();
            if (tok.k == T_ID)
                add_import(list, n, tok.s);
            while (!is(";") && tok.k != T_EOF)
                next();
        } else if (is("IMPORT")) {
            next();
            while (tok.k == T_ID) {
                add_import(list, n, tok.s);
                next();
                if (!accept(","))
                    break;
            }
        } else
            next();
    }
    src = osrc;
    srcname = osrcname;
    pos = opos;
    curline = ocur;
    errline = oerr;
    tok = otok;
}

static DM *get_dm(const char *name)
{
    DM *m;
    for (m = dms; m; m = m->next)
        if (!strcmp(m->name, name))
            return m;
    m = calloc(1, sizeof *m);
    m->name = fmt("%s", name);
    m->defpath = findfile(name, ".def");
    m->modpath = findfile(name, ".mod");
    m->cpath = findfile(name, ".c");
    if (m->defpath)
        scan_imports(m->defpath, m->defimps, &m->ndi);
    if (m->modpath)
        scan_imports(m->modpath, m->modimps, &m->nmi);
    m->next = dms;
    dms = m;
    return m;
}

/* a set of modules, in the order in which they were found */
typedef struct Set {
    DM *m[64];
    int n;
} Set;

static int in_set(Set *s, DM *m)
{
    int i;
    for (i = 0; i < s->n; i++)
        if (s->m[i] == m)
            return 1;
    return 0;
}

static void put(Set *s, DM *m)
{
    if (in_set(s, m))
        return;
    if (s->n >= 64)
        die("too many modules");
    s->m[s->n++] = m;
}

/* the modules whose .def is read when m is imported: m, and what its .def
   imports (a circular import is an error) */
static void def_closure(DM *m, Set *s)
{
    int i;
    if (m->visiting)
        die("circular import of the module %s", m->name);
    if (in_set(s, m))
        return;
    if (!m->defpath)
        die("the definition module %s was not found (%s.def)", m->name, m->name);
    m->visiting = 1;
    for (i = 0; i < m->ndi; i++)
        def_closure(get_dm(m->defimps[i]), s);
    m->visiting = 0;
    put(s, m);
}

/* the modules that the program needs: those imported by the .mod files */
static void link_closure(DM *m, Set *s)
{
    int i;
    for (i = 0; i < m->nmi; i++) {
        DM *x = get_dm(m->modimps[i]);
        if (in_set(s, x))
            continue;
        put(s, x);
        link_closure(x, s);
    }
}

static const char *base_of(const char *path, char *buf, size_t n)
{
    const char *b = strrchr(path, '/');
    size_t k;
    b = b ? b + 1 : path;
    snprintf(buf, n, "%s", b);
    k = strlen(buf);
    if (k > 4 && !strcmp(buf + k - 4, ".mod"))
        buf[k - 4] = 0;
    return buf;
}

/* the headers and the .def files that the translation of a module with the
   .def closure s reads */
static void print_defs(Set *s)
{
    int i;
    for (i = 0; i < s->n; i++)
        fprintf(out, " %s", s->m[i]->defpath);
}

static void print_hdrs(Set *s)
{
    int i;
    for (i = 0; i < s->n; i++)
        fprintf(out, " %s.h", s->m[i]->name);
}

/* the closure of the .def files of the module m (when m is a module with a
   .def) and of the modules that its .mod imports */
static void build_closure(DM *m, int own_def, Set *s)
{
    int i;
    if (own_def)
        def_closure(m, s);
    for (i = 0; i < m->nmi; i++)
        def_closure(get_dm(m->modimps[i]), s);
}

int deps_main(void)
{
    char name[256];
    DM *top, *m;
    Set link, all, tmp;
    int i, j, is_program;
    char *m2flags = "";
    size_t k = strlen(srcname);
    if (k < 5 || strcmp(srcname + k - 4, ".mod"))
        die("-M needs a .mod file (a program module or an implementation module)");
    base_of(srcname, name, sizeof name);
    top = calloc(1, sizeof *top);
    top->name = fmt("%s", name);
    top->modpath = (char *)srcname;
    top->defpath = findfile(name, ".def");
    scan_imports(srcname, top->modimps, &top->nmi);
    if (top->defpath)
        scan_imports(top->defpath, top->defimps, &top->ndi);
    top->next = dms;
    dms = top;
    is_program = top->defpath == NULL;

    memset(&link, 0, sizeof link);
    memset(&all, 0, sizeof all);
    link_closure(top, &link);
    /* every module that is translated, linked, or whose header is needed */
    memset(&tmp, 0, sizeof tmp);
    build_closure(top, !is_program, &tmp);
    for (i = 0; i < tmp.n; i++)
        put(&all, tmp.m[i]);
    for (i = 0; i < link.n; i++) {
        Set t;
        memset(&t, 0, sizeof t);
        if (link.m[i]->defpath || link.m[i]->modpath)
            build_closure(link.m[i], link.m[i]->defpath != NULL, &t);
        for (j = 0; j < t.n; j++)
            put(&all, t.m[j]);
        put(&all, link.m[i]);
    }

    for (i = 1; i < nincdirs; i++)
        m2flags = fmt("%s -I %s", m2flags, incdirs[i]);
    fprintf(out, "# made by arinm2c -M %s: do not edit\n", srcname);
    fprintf(out, "M2C = arinm2c\nM2FLAGS =%s\n", m2flags);
    fprintf(out, "CFLAGS = -std=c11 -fwrapv -Wall -Wno-unused-label\n");
    fprintf(out, "LDFLAGS =\n\n.PHONY: all clean\nall: %s%s\n\n", name,
            is_program ? "" : ".o");

    /* the objects */
    fprintf(out, "OBJS =");
    if (!is_program)
        fprintf(out, " %s.o", name);
    for (i = 0; i < link.n; i++)
        if (link.m[i]->modpath || link.m[i]->cpath)
            fprintf(out, " %s.o", link.m[i]->name);
    fprintf(out, "\n\n");
    /* the C files made by arinm2c must agree on the version of every
       definition module (arinm2c -V) */
    fprintf(out, "GENC = %s.c", name);
    for (i = 0; i < link.n; i++)
        if (link.m[i]->modpath)
            fprintf(out, " %s.c", link.m[i]->name);
    fprintf(out, "\n\n");
    if (is_program) {
        fprintf(out, "%s: %s.o $(OBJS)\n", name, name);
        fprintf(out, "\t$(M2C) -V $(GENC)\n");
        fprintf(out, "\t$(CC) $(CFLAGS) -o $@ %s.o $(OBJS) $(LDFLAGS)\n\n", name);
    }

    /* the top module: its C file and object */
    {
        Set d;
        memset(&d, 0, sizeof d);
        build_closure(top, !is_program, &d);
        fprintf(out, "%s.c: %s", name, srcname);
        print_defs(&d);
        fprintf(out, "\n\t$(M2C) $(M2FLAGS) %s -o $@\n\n", srcname);
        fprintf(out, "%s.o: %s.c", name, name);
        print_hdrs(&d);
        fprintf(out, "\n\t$(CC) $(CFLAGS) -I. -c %s.c -o $@\n\n", name);
    }

    /* the other modules */
    for (i = 0; i < all.n; i++) {
        Set d;
        m = all.m[i];
        if (m == top)
            continue;
        memset(&d, 0, sizeof d);
        if (m->defpath) {
            /* the header */
            Set h;
            memset(&h, 0, sizeof h);
            def_closure(m, &h);
            fprintf(out, "%s.h:", m->name);
            print_defs(&h);
            fprintf(out, "\n\t$(M2C) $(M2FLAGS) %s -o $@\n\n", m->defpath);
        }
        if (m->modpath) {
            build_closure(m, m->defpath != NULL, &d);
            fprintf(out, "%s.c: %s", m->name, m->modpath);
            print_defs(&d);
            fprintf(out, "\n\t$(M2C) $(M2FLAGS) %s -o $@\n\n", m->modpath);
            fprintf(out, "%s.o: %s.c", m->name, m->name);
            print_hdrs(&d);
            fprintf(out, "\n\t$(CC) $(CFLAGS) -I. -c %s.c -o $@\n\n", m->name);
        } else if (m->cpath) {
            fprintf(out, "%s.o: %s %s.h\n", m->name, m->cpath, m->name);
            fprintf(out, "\t$(CC) $(CFLAGS) -I. -c %s -o $@\n\n", m->cpath);
        }
    }

    /* clean */
    fprintf(out, "clean:\n\trm -f %s%s $(OBJS)", is_program ? name : "",
            is_program ? " " : "");
    fprintf(out, " %s.c %s.o", name, name);
    for (i = 0; i < all.n; i++) {
        m = all.m[i];
        if (m == top)
            continue;
        if (m->defpath)
            fprintf(out, " %s.h", m->name);
        if (m->modpath)
            fprintf(out, " %s.c", m->name);
    }
    fprintf(out, "\n");
    return 0;
}

/* the lines "m2def NAME MD5" in comments that the translator writes at the end
   of a C file */
int versions_main(int n, char **files)
{
    struct { char *name, *md5, *file; } seen[256];
    int ns = 0, i, bad = 0;
    for (i = 0; i < n; i++) {
        char *text = readfile(files[i]), *p;
        if (!text) {
            perror(files[i]);
            return 1;
        }
        for (p = text; (p = strstr(p, "/* m2def ")) != NULL; p++) {
            char name[128], md5[64];
            int j;
            if (p != text && p[-1] != '\n')
                continue;
            if (sscanf(p, "/* m2def %127s %63s */", name, md5) != 2)
                continue;
            for (j = 0; j < ns; j++)
                if (!strcmp(seen[j].name, name))
                    break;
            if (j == ns) {
                if (ns >= 256)
                    die("too many modules");
                seen[ns].name = fmt("%s", name);
                seen[ns].md5 = fmt("%s", md5);
                seen[ns].file = files[i];
                ns++;
            } else if (strcmp(seen[j].md5, md5)) {
                fprintf(stderr,
                        "the definition module %s has two versions: %s (%s) and %s (%s)\n",
                        name, seen[j].md5, seen[j].file, md5, files[i]);
                bad = 1;
            }
        }
    }
    return bad;
}
