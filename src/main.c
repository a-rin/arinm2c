/* main: the command line */
#include "arinm2c.h"

int main(int argc, char **argv)
{
    char *text, *slash;
    const char *outname = NULL;
    int i, want_deps = 0;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-M"))
            want_deps = 1;
        else if (!strcmp(argv[i], "-o") && i + 1 < argc)
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
        fprintf(stderr, "usage: arinm2c [-I dir] [-M] file.{mod,def} [-o file]\n");
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
    if (want_deps) {
        int rc = deps_main();
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
