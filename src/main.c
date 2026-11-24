/* main: the command line */
#include "arinm2c.h"

int main(int argc, char **argv)
{
    char *text, *slash;
    const char *outname = NULL;
    int i, want_deps = 0, want_manifest = 0, nlibnames = 0;
    char **libnames = NULL;
    if (argc > 1 && !strcmp(argv[1], "-V"))
        return versions_main(argc - 2, argv + 2);
    incdirs = grow(NULL, 0, sizeof *incdirs);   /* the slot 0: the source directory */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-M"))
            want_deps = 1;
        else if (!strcmp(argv[i], "-P"))
            want_manifest = 1;
        else if (!strcmp(argv[i], "-L") && i + 1 < argc) {
            libdirs = grow(libdirs, nlibdirs, sizeof *libdirs);
            libdirs[nlibdirs++] = argv[++i];
        }
        else if (!strcmp(argv[i], "-l") && i + 1 < argc) {
            libnames = grow(libnames, nlibnames, sizeof *libnames);
            libnames[nlibnames++] = argv[++i];
        }
        else if (!strcmp(argv[i], "-o") && i + 1 < argc)
            outname = argv[++i];
        else if (!strcmp(argv[i], "-I") && i + 1 < argc) {
            incdirs = grow(incdirs, nincdirs + 1, sizeof *incdirs);
            incdirs[++nincdirs] = argv[++i];
        } else if (!srcname)
            srcname = argv[i];
        else {
            srcname = NULL;
            break;
        }
    }
    if (!srcname) {
        fprintf(stderr, "usage: arinm2c [-I dir] [-L dir] [-l lib] [-M|-P] file.{mod,def,m2lib} [-o file]\n       arinm2c -V file...\n");
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
    for (i = 0; i < nlibnames; i++)
        lib_use(libnames[i]);
    if (outname && !(out = fopen(outname, "w"))) {
        perror(outname);
        return 1;
    }
    if (want_deps || want_manifest) {
        int rc = want_manifest ? manifest_main() : deps_main();
        if (outname)
            fclose(out);
        return rc;
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
