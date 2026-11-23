/* lex: the scanner: tokens of the Modula-2 source */
#ifndef ARINM2C_LEX_H
#define ARINM2C_LEX_H

#include "util.h"

enum { T_EOF, T_ID, T_KW, T_SYM, T_NUM, T_STR, T_REAL };

typedef struct {
    int k;
    char *s;
    long long v;
    int line;
} Tok;

extern Tok tok;

extern const char *src;

extern size_t pos;

extern int curline;

void next(void);

int is(const char *s);

int accept(const char *s);

void expect(const char *s);

char *ident(void);

#endif
