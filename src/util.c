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

/* ponytail: the memory of the translator is never freed (a one-shot process);
   ceiling: a source of many million lines; upgrade: an arena that is freed per module */
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

char *types_text;
int hoist_types;

/* the declaration of a type: in a procedure it is hoisted out of the function */
void emit_t(const char *f, ...)
{
    va_list ap, ap2;
    int n;
    char *line;
    va_start(ap, f);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, f, ap);
    va_end(ap);
    line = malloc((size_t)n + 1);
    if (!line)
        exit(1);
    vsnprintf(line, (size_t)n + 1, f, ap2);
    va_end(ap2);
    if (!hoist_types) {
        emit("%s", line);
        return;
    }
    types_text = fmt("%s%s\n", types_text ? types_text : "", line);
}

FILE *cap_begin(void)
{
    FILE *old = out;
    out = tmpfile();
    if (!out) {
        perror("tmpfile");
        exit(1);
    }
    return old;
}

char *cap_end(FILE *old)
{
    long n;
    char *buf;
    fflush(out);
    n = ftell(out);
    rewind(out);
    buf = malloc((size_t)n + 1);
    if (!buf || fread(buf, 1, (size_t)n, out) != (size_t)n)
        exit(1);
    buf[n] = 0;
    fclose(out);
    out = old;
    return buf;
}

void *grow(void *list, int n, size_t size)
{
    list = realloc(list, ((size_t)n + 1) * size);
    if (!list)
        exit(1);
    return list;
}
