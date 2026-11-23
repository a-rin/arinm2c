/* util: messages, C text formatting and the output of the C program */
#ifndef ARINM2C_UTIL_H
#define ARINM2C_UTIL_H

extern const char *srcname;

extern FILE *out;

extern int ind;

extern int errline;

extern int quiet;

void die(const char *f, ...) __attribute__((noreturn));

char *fmt(const char *f, ...);

void emit(const char *f, ...);

char *cstr(const char *s);

char *cchar(unsigned char c);

/* the C text of the types declared in procedures (it goes before the C
   functions: C has no local functions) */
extern char *types_text;
extern int hoist_types;
void emit_t(const char *f, ...);

/* write the text of the following emit calls to a string instead of the file */
FILE *cap_begin(void);
char *cap_end(FILE *old);

#endif
