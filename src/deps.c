/* deps: arinm2c -M Main.mod writes a Makefile that builds the program */
#include "arinm2c.h"

/* A module as the Makefile sees it: its files and the modules that they
   import. The imports are found by a scan of the tokens (IMPORT and FROM), the
   source is not translated. */
typedef struct DM {
    char *name;
    char *defpath, *modpath, *cpath;    /* the files that exist, or NULL */
    char **defimps, **modimps;          /* the modules imported by .def, .mod */
    int ndi, nmi;
    int visiting, linked;
    Lib *lib;                           /* the library that has its .def, or NULL */
    struct DM *next;
} DM;

static DM *dms;

static void add_import(char ***list, int *n, const char *name)
{
    int i;
    if (!strcmp(name, "SYSTEM") || !findfile(name, ".def"))
        return;         /* a name that is not a module */
    for (i = 0; i < *n; i++)
        if (!strcmp((*list)[i], name))
            return;
    *list = grow(*list, *n, sizeof **list);
    (*list)[(*n)++] = fmt("%s", name);
}

/* the modules named by FROM m IMPORT and IMPORT m in a source file */
static void scan_imports(const char *path, char ***list, int *n)
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
        m->lib = lib_of_path(m->defpath);
    if (m->defpath)
        scan_imports(m->defpath, &m->defimps, &m->ndi);
    if (m->modpath)
        scan_imports(m->modpath, &m->modimps, &m->nmi);
    m->next = dms;
    dms = m;
    return m;
}

/* a set of modules, in the order in which they were found */
typedef struct Set {
    DM **m;
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
    s->m = grow(s->m, s->n, sizeof *s->m);
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
        if (s->m[i]->lib)
            fprintf(out, " %s/%s.h", s->m[i]->lib->dir, s->m[i]->name);
        else
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

/* link: the modules whose code is linked (those imported by the .mod files);
   all: every module that is translated, linked, or whose header is needed */
static void sets_of(DM *top, int own_def, Set *link, Set *all)
{
    Set tmp;
    int i, j;
    memset(link, 0, sizeof *link);
    memset(all, 0, sizeof *all);
    link_closure(top, link);
    memset(&tmp, 0, sizeof tmp);
    build_closure(top, own_def, &tmp);
    for (i = 0; i < tmp.n; i++)
        put(all, tmp.m[i]);
    for (i = 0; i < link->n; i++) {
        Set t;
        memset(&t, 0, sizeof t);
        if (link->m[i]->defpath || link->m[i]->modpath)
            build_closure(link->m[i], link->m[i]->defpath != NULL, &t);
        for (j = 0; j < t.n; j++)
            put(all, t.m[j]);
        put(all, link->m[i]);
    }
}

/* the library as a module that imports its modules */
static DM *top_of_lib(Lib *L)
{
    DM *top = calloc(1, sizeof *top);
    int i;
    top->name = "(library)";
    for (i = 0; i < L->nmods; i++) {
        if (!findfile(L->mods[i], ".def"))
            die("the module %s of the library %s has no %s.def", L->mods[i], L->name,
                L->mods[i]);
        top->modimps = grow(top->modimps, top->nmi, sizeof *top->modimps);
        top->modimps[top->nmi++] = fmt("%s", L->mods[i]);
    }
    top->next = dms;
    dms = top;
    return top;
}

static int is_own(Lib *L, DM *m)
{
    int i;
    for (i = 0; i < L->nmods; i++)
        if (!strcmp(L->mods[i], m->name))
            return 1;
    return 0;
}

/* what the library imports must be in it or in a library that it requires */
static void check_members(Lib *L, Set *link)
{
    int i;
    for (i = 0; i < link->n; i++) {
        DM *m = link->m[i];
        if (!m->lib && !is_own(L, m))
            die("the module %s is imported in the library %s, but it is not in the "
                "library (add 'module %s' to %s) and not in a required library",
                m->name, L->name, m->name, L->mpath);
    }
}

static void setup_libs(Lib *L)
{
    int i;
    for (i = 0; i < L->nreq; i++)
        lib_use(L->req[i]);
}

int deps_main(void)
{
    char name[256];
    DM *top, *m;
    Set link, all;
    int i, is_program = 0, islib;
    Lib *L = NULL;
    char *m2flags = "", *incs = "", *ldlibs = "", *mfiles = "";
    size_t k = strlen(srcname);
    islib = k > 6 && !strcmp(srcname + k - 6, ".m2lib");
    if (islib) {
        L = lib_parse(srcname);
        setup_libs(L);
        snprintf(name, sizeof name, "%s", L->name);
        top = top_of_lib(L);
    } else {
        if (k < 5 || strcmp(srcname + k - 4, ".mod"))
            die("-M needs a .mod file (a program module or an implementation "
                "module) or a .m2lib file (a library)");
        base_of(srcname, name, sizeof name);
        top = calloc(1, sizeof *top);
        top->name = fmt("%s", name);
        top->modpath = (char *)srcname;
        top->defpath = findfile(name, ".def");
        scan_imports(srcname, &top->modimps, &top->nmi);
        if (top->defpath)
            scan_imports(top->defpath, &top->defimps, &top->ndi);
        top->next = dms;
        dms = top;
        is_program = top->defpath == NULL;
    }
    sets_of(top, !is_program && !islib, &link, &all);
    if (islib)
        check_members(L, &link);

    /* the directories of the libraries are found again with -L and -l */
    for (i = 1; i < nincdirs; i++) {
        int j, isl = 0;
        for (j = 0; j < nlibs; j++)
            isl |= !strcmp(incdirs[i], libs[j]->dir);
        if (!isl)
            m2flags = fmt("%s -I %s", m2flags, incdirs[i]);
    }
    for (i = 0; i < nlibdirs; i++)
        m2flags = fmt("%s -L %s", m2flags, libdirs[i]);
    for (i = 0; i < nlibs; i++)
        m2flags = fmt("%s -l %s", m2flags, libs[i]->name);
    for (i = 0; i < nlibs; i++) {
        incs = fmt("%s -I %s", incs, libs[i]->dir);
        ldlibs = fmt("%s -L %s -l%s", ldlibs, libs[i]->dir, libs[i]->name);
        mfiles = fmt("%s %s", mfiles, libs[i]->mpath);
    }
    fprintf(out, "# made by arinm2c -M %s: do not edit\n", srcname);
    fprintf(out, "M2C = arinm2c\nM2FLAGS =%s\n", m2flags);
    fprintf(out, "CFLAGS = -std=c11 -fwrapv -Wall -Wno-unused-label\n");
    fprintf(out, "INCS =%s\nLDFLAGS =\nLIBS =%s\nLIBFILES =%s\n\n", incs, ldlibs,
            mfiles);
    fprintf(out, ".PHONY: all clean%s\n", islib ? " install" : "");
    if (islib)
        fprintf(out, "all: lib%s.a\n\n", name);
    else
        fprintf(out, "all: %s%s\n\n", name, is_program ? "" : ".o");

    /* the objects */
    fprintf(out, "OBJS =");
    if (!is_program && !islib)
        fprintf(out, " %s.o", name);
    for (i = 0; i < link.n; i++)
        if (link.m[i]->modpath || link.m[i]->cpath)
            fprintf(out, " %s.o", link.m[i]->name);
    fprintf(out, "\n\n");
    /* the C files made by arinm2c must agree on the version of every
       definition module (arinm2c -V) */
    fprintf(out, "GENC =");
    if (!islib)
        fprintf(out, " %s.c", name);
    for (i = 0; i < link.n; i++)
        if (link.m[i]->modpath)
            fprintf(out, " %s.c", link.m[i]->name);
    fprintf(out, "\n\n");
    if (is_program) {
        fprintf(out, "%s: %s.o $(OBJS)\n", name, name);
        fprintf(out, "\t$(M2C) -V $(GENC) $(LIBFILES)\n");
        fprintf(out, "\t$(CC) $(CFLAGS) -o $@ %s.o $(OBJS) $(LDFLAGS) $(LIBS)\n\n",
                name);
    }
    if (islib) {
        fprintf(out, "lib%s.a: $(OBJS)\n", name);
        fprintf(out, "\t$(M2C) -V $(GENC) $(LIBFILES)\n");
        fprintf(out, "\trm -f $@\n\tar rcs $@ $(OBJS)\n\n");
        /* install: one directory per library, with what a program needs */
        fprintf(out, "PREFIX = /usr/local\nDEST = $(PREFIX)/lib/m2/%s\n\n", name);
        fprintf(out, "install: lib%s.a", name);
        for (i = 0; i < L->nmods; i++)
            fprintf(out, " %s.h", L->mods[i]);
        fprintf(out, "\n\tmkdir -p $(DEST)\n\tcp lib%s.a", name);
        for (i = 0; i < L->nmods; i++)
            fprintf(out, " %s.h %s", L->mods[i], get_dm(L->mods[i])->defpath);
        fprintf(out, " $(DEST)/\n");
        fprintf(out, "\t$(M2C) $(M2FLAGS) -P %s -o $(DEST)/%s.m2lib\n\n", srcname,
                name);
    }

    /* the top module: its C file and object */
    if (!islib) {
        Set d;
        memset(&d, 0, sizeof d);
        build_closure(top, !is_program, &d);
        fprintf(out, "%s.c: %s", name, srcname);
        print_defs(&d);
        fprintf(out, "\n\t$(M2C) $(M2FLAGS) %s -o $@\n\n", srcname);
        fprintf(out, "%s.o: %s.c", name, name);
        print_hdrs(&d);
        fprintf(out, "\n\t$(CC) $(CFLAGS) $(INCS) -I. -c %s.c -o $@\n\n", name);
    }

    /* the other modules (not those of a library that is used: they are built) */
    for (i = 0; i < all.n; i++) {
        Set d;
        m = all.m[i];
        if (m == top || m->lib)
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
            fprintf(out, "\n\t$(CC) $(CFLAGS) $(INCS) -I. -c %s.c -o $@\n\n", m->name);
        } else if (m->cpath) {
            fprintf(out, "%s.o: %s %s.h\n", m->name, m->cpath, m->name);
            fprintf(out, "\t$(CC) $(CFLAGS) $(INCS) -I. -c %s -o $@\n\n", m->cpath);
        }
    }

    /* clean */
    if (islib)
        fprintf(out, "clean:\n\trm -f lib%s.a $(OBJS)", name);
    else
        fprintf(out, "clean:\n\trm -f %s%s $(OBJS) %s.c %s.o", is_program ? name : "",
                is_program ? " " : "", name, name);
    for (i = 0; i < all.n; i++) {
        m = all.m[i];
        if (m == top || m->lib)
            continue;
        if (m->defpath)
            fprintf(out, " %s.h", m->name);
        if (m->modpath)
            fprintf(out, " %s.c", m->name);
    }
    fprintf(out, "\n");
    return 0;
}

/* arinm2c -P NAME.m2lib: the library file that is installed: the versions of
   its modules and of all the .def files that it uses are added */
int manifest_main(void)
{
    Lib *L = lib_parse(srcname);
    DM *top;
    Set link, all;
    int i;
    char md5[33];
    setup_libs(L);
    top = top_of_lib(L);
    sets_of(top, 0, &link, &all);
    check_members(L, &link);
    fprintf(out, "# made by arinm2c -P %s: do not edit\nlibrary %s\nabi %d\n", srcname,
            L->name, M2_ABI);
    for (i = 0; i < L->nmods; i++) {
        def_md5(readfile(get_dm(L->mods[i])->defpath), md5);
        fprintf(out, "module %s %s\n", L->mods[i], md5);
    }
    /* every other .def that was read, also the ones that these import */
    for (i = 0; i < all.n; i++) {
        if (is_own(L, all.m[i]))
            continue;
        def_md5(readfile(all.m[i]->defpath), md5);
        fprintf(out, "uses %s %s\n", all.m[i]->name, md5);
    }
    for (i = 0; i < L->nreq; i++)
        fprintf(out, "requires %s\n", L->req[i]);
    return 0;
}

typedef struct Seen {
    char *name, *md5, *file;
} Seen;

static Seen *seen;

static int nseen;

/* a module with the version md5 is in the file: 1 if it has another version
   in an earlier file */
static int note(const char *name, const char *md5, char *file)
{
    int j;
    for (j = 0; j < nseen; j++)
        if (!strcmp(seen[j].name, name))
            break;
    if (j == nseen) {
        seen = grow(seen, nseen, sizeof *seen);
        seen[nseen].name = fmt("%s", name);
        seen[nseen].md5 = fmt("%s", md5);
        seen[nseen].file = file;
        nseen++;
        return 0;
    }
    if (!strcmp(seen[j].md5, md5))
        return 0;
    fprintf(stderr, "the definition module %s has two versions: %s (%s) and %s (%s)\n",
            name, seen[j].md5, seen[j].file, md5, file);
    return 1;
}

/* the lines "m2def NAME MD5" in comments that the translator writes at the end
   of a C file, and the lines "module NAME MD5" and "uses NAME MD5" of a .m2lib
   file (a library has no C files) */
int versions_main(int n, char **files)
{
    int i, bad = 0;
    for (i = 0; i < n; i++) {
        char *text = readfile(files[i]), *p;
        size_t k = strlen(files[i]);
        if (!text) {
            perror(files[i]);
            return 1;
        }
        if (k > 6 && !strcmp(files[i] + k - 6, ".m2lib")) {
            for (p = text; *p; p++) {
                char kw[32], name[128], md5[64];
                if ((p == text || p[-1] == '\n') &&
                    sscanf(p, "%31s %127s %63s", kw, name, md5) == 3 &&
                    (!strcmp(kw, "module") || !strcmp(kw, "uses")))
                    bad |= note(name, md5, files[i]);
            }
            continue;
        }
        for (p = text; (p = strstr(p, "/* m2def ")) != NULL; p++) {
            char name[128], md5[64];
            if (p != text && p[-1] != '\n')
                continue;
            if (sscanf(p, "/* m2def %127s %63s */", name, md5) != 2)
                continue;
            bad |= note(name, md5, files[i]);
        }
    }
    return bad;
}
