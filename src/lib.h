/* lib: libraries. A library is a directory DIR/NAME/ with NAME.m2lib, libNAME.a,
   and the .def and .h files of its modules (arinm2c -M NAME.m2lib builds it) */
#ifndef ARINM2C_LIB_H
#define ARINM2C_LIB_H

/* the version of the C that the translator writes: a library built with
   another one does not fit */
#define M2_ABI 1

typedef struct Lib {
    char *name, *dir, *mpath;   /* mpath: the path of NAME.m2lib */
    char **mods;                /* module NAME [md5] */
    int nmods;
    char **req;                 /* requires NAME */
    int nreq;
    int abi;
} Lib;

extern char **libdirs;          /* -L */
extern int nlibdirs;
extern Lib **libs;              /* the libraries in use, in the order of the link */
extern int nlibs;

/* read a NAME.m2lib (the lines: library, abi, module, uses, requires) */
Lib *lib_parse(const char *path);

/* -l NAME: find the library in the -L directories, add its directory to the
   search path of the .def files, and do the same for the libraries it requires */
void lib_use(const char *name);

/* the library that has this path (of a .def) in its directory, or NULL */
Lib *lib_of_path(const char *path);

#endif
