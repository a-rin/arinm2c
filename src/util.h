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

#endif
