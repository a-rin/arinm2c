/* lib: libraries (see lib.h) */
#include "arinm2c.h"

char **libdirs;
int nlibdirs;
Lib **libs;
int nlibs;

Lib *lib_parse(const char *path)
{
    char *text = readfile(path), *line, *nl, *slash;
    Lib *L;
    if (!text) {
        perror(path);
        exit(1);
    }
    L = calloc(1, sizeof *L);
    L->mpath = fmt("%s", path);
    L->dir = fmt("%s", path);
    slash = strrchr(L->dir, '/');
    if (slash)
        *slash = 0;
    else
        L->dir = ".";
    for (line = text; *line; line = nl) {
        char kw[32], a[128], b[64], *h;
        int k;
        nl = strchr(line, '\n');
        if (nl)
            *nl++ = 0;
        else
            nl = line + strlen(line);
        h = strchr(line, '#');
        if (h)
            *h = 0;
        k = sscanf(line, "%31s %127s %63s", kw, a, b);
        if (k < 1)
            continue;
        if (k < 2)
            die("%s: '%s' needs a name", path, kw);
        if (!strcmp(kw, "library"))
            L->name = fmt("%s", a);
        else if (!strcmp(kw, "abi"))
            L->abi = atoi(a);
        else if (!strcmp(kw, "module")) {
            L->mods = grow(L->mods, L->nmods, sizeof *L->mods);
            L->mods[L->nmods++] = fmt("%s", a);
        } else if (!strcmp(kw, "requires")) {
            L->req = grow(L->req, L->nreq, sizeof *L->req);
            L->req[L->nreq++] = fmt("%s", a);
        } else if (strcmp(kw, "uses"))
            die("%s: unknown line '%s'", path, kw);
    }
    if (!L->name)
        die("%s: the line 'library NAME' is missing", path);
    return L;
}

void lib_use(const char *name)
{
    int i;
    char *path = NULL;
    Lib *L;
    for (i = 0; i < nlibs; i++)
        if (!strcmp(libs[i]->name, name))
            return;
    for (i = 0; i < nlibdirs && !path; i++) {
        char *p = fmt("%s/%s/%s.m2lib", libdirs[i], name, name);
        FILE *f = fopen(p, "rb");
        if (f) {
            fclose(f);
            path = p;
        }
    }
    if (!path)
        die("the library %s was not found (-L dir: dir/%s/%s.m2lib)", name, name, name);
    L = lib_parse(path);
    if (strcmp(L->name, name))
        die("%s: the library is named %s, not %s", path, L->name, name);
    if (L->abi != M2_ABI)
        die("%s: the library has the abi %d, this translator writes %d: build the "
            "library again", path, L->abi, M2_ABI);
    /* a module name is one name: it must not be in a directory or library that
       is already searched */
    for (i = 0; i < L->nmods; i++) {
        char *o = findfile(L->mods[i], ".def");
        if (o)
            die("the module %s of the library %s is also found at %s", L->mods[i],
                name, o);
    }
    incdirs = grow(incdirs, nincdirs, sizeof *incdirs);
    incdirs[nincdirs++] = L->dir;
    /* ponytail: a library is put after the ones that use it only in the order
       found (depth first); ceiling: two libraries that require a third one in a
       diamond may be in the wrong order for a static link; upgrade: a topological
       sort */
    libs = grow(libs, nlibs, sizeof *libs);
    libs[nlibs++] = L;
    for (i = 0; i < L->nreq; i++)
        lib_use(L->req[i]);
}

Lib *lib_of_path(const char *path)
{
    int i;
    for (i = 0; i < nlibs; i++) {
        size_t n = strlen(libs[i]->dir);
        if (!strncmp(path, libs[i]->dir, n) && path[n] == '/')
            return libs[i];
    }
    return NULL;
}
