/* util: messages, C text formatting and the output of the C program */
#include "arinm2c.h"

const char *srcname;

FILE *out;

int ind;

int errline;

int quiet;      /* > 0: nothing is written (a definition module is only read) */

void die(const char *f, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d: error: ", srcname, errline);
    va_start(ap, f);
    vfprintf(stderr, f, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

char *fmt(const char *f, ...)
{
    va_list ap, ap2;
    int n;
    char *p;
    va_start(ap, f);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, f, ap);
    va_end(ap);
    p = malloc((size_t)n + 1);
    if (!p)
        exit(1);
    vsnprintf(p, (size_t)n + 1, f, ap2);
    va_end(ap2);
    return p;
}

void emit(const char *f, ...)
{
    va_list ap;
    int i;
    if (quiet)
        return;
    for (i = 0; i < ind; i++)
        fputs("  ", out);
    va_start(ap, f);
    vfprintf(out, f, ap);
    va_end(ap);
    fputc('\n', out);
}

/* the text of a C string literal for the bytes s */
char *cstr(const char *s)
{
    size_t n = strlen(s) * 4 + 3, k = 0;
    char *p = malloc(n);
    p[k++] = '"';
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\')
            k += (size_t)sprintf(p + k, "\\%c", c);
        else if (c < 32 || c > 126)
            k += (size_t)sprintf(p + k, "\\%03o", c);
        else
            p[k++] = (char)c;
    }
    p[k++] = '"';
    p[k] = 0;
    return p;
}

char *cchar(unsigned char c)
{
    if (c == '\'' || c == '\\')
        return fmt("'\\%c'", c);
    if (c < 32 || c > 126)
        return fmt("'\\%03o'", c);
    return fmt("'%c'", c);
}
