/* lex: the scanner: tokens of the Modula-2 source */
#include "arinm2c.h"

Tok tok;

const char *src;

size_t pos;

int curline = 1;

static const char *kws[] = {
    "AND", "ARRAY", "BEGIN", "BY", "CASE", "CONST", "DEFINITION", "DIV", "DO",
    "ELSE", "ELSIF", "END", "EXIT", "EXPORT", "FOR", "FROM", "IF",
    "IMPLEMENTATION", "IMPORT", "IN", "LOOP", "MOD", "MODULE", "NOT", "OF",
    "OR", "POINTER", "PROCEDURE", "QUALIFIED", "RECORD", "REPEAT", "RETURN",
    "SET", "THEN", "TO", "TYPE", "UNTIL", "VAR", "WHILE", "WITH", 0};

static int iskw(const char *s)
{
    int i;
    for (i = 0; kws[i]; i++)
        if (!strcmp(kws[i], s))
            return 1;
    return 0;
}

/* a real number: digits . digits [E [+|-] digits] */
static int real_literal(void)
{
    size_t q = pos;
    char *p;
    while (isdigit((unsigned char)src[q]))
        q++;
    if (q == pos || src[q] != '.' || !isdigit((unsigned char)src[q + 1]))
        return 0;
    q++;
    while (isdigit((unsigned char)src[q]))
        q++;
    if (src[q] == 'E') {
        size_t r = q + 1;
        if (src[r] == '+' || src[r] == '-')
            r++;
        if (isdigit((unsigned char)src[r])) {
            while (isdigit((unsigned char)src[r]))
                r++;
            q = r;
        }
    }
    p = malloc(q - pos + 1);
    memcpy(p, src + pos, q - pos);
    p[q - pos] = 0;
    tok.s = p;
    tok.k = T_REAL;
    pos = q;
    return 1;
}

void next(void)
{
    size_t st;
    for (;;) {
        char c = src[pos];
        if (c == '\n') {
            curline++;
            pos++;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\f') {
            pos++;
        } else if (c == '(' && src[pos + 1] == '*') {
            int depth = 1;
            errline = curline;
            pos += 2;
            while (depth > 0) {
                if (!src[pos])
                    die("unterminated comment");
                if (src[pos] == '(' && src[pos + 1] == '*') {
                    depth++;
                    pos += 2;
                } else if (src[pos] == '*' && src[pos + 1] == ')') {
                    depth--;
                    pos += 2;
                } else {
                    if (src[pos] == '\n')
                        curline++;
                    pos++;
                }
            }
        } else
            break;
    }
    tok.line = errline = curline;
    tok.s = "";
    tok.v = 0;
    st = pos;
    if (!src[pos]) {
        tok.k = T_EOF;
    } else if (isalpha((unsigned char)src[pos])) {
        char *p;
        while (isalnum((unsigned char)src[pos]))
            pos++;
        p = malloc(pos - st + 1);
        memcpy(p, src + st, pos - st);
        p[pos - st] = 0;
        tok.s = p;
        tok.k = iskw(p) ? T_KW : T_ID;
    } else if (real_literal()) {
        /* tok is set */
    } else if (isdigit((unsigned char)src[pos])) {
        size_t n;
        char last, *p;
        while (isdigit((unsigned char)src[pos]) ||
               (src[pos] >= 'A' && src[pos] <= 'F'))
            pos++;
        n = pos - st;
        p = malloc(n + 2);
        memcpy(p, src + st, n);
        p[n] = 0;
        last = p[n - 1];
        if (src[pos] == 'H') {
            pos++;
            tok.v = strtoll(p, NULL, 16);
            tok.k = T_NUM;
        } else if (last == 'C' && n > 1) {
            p[n - 1] = 0;
            tok.v = strtoll(p, NULL, 8);
            tok.k = T_STR;
            tok.s = fmt("%c", (int)tok.v);
        } else {
            size_t i;
            for (i = 0; i < n; i++)
                if (!isdigit((unsigned char)p[i]))
                    die("bad number");
            tok.v = strtoll(p, NULL, 10);
            tok.k = T_NUM;
        }
    } else if (src[pos] == '\'' || src[pos] == '"') {
        char q = src[pos++];
        char *p;
        st = pos;
        while (src[pos] != q) {
            if (!src[pos] || src[pos] == '\n')
                die("unterminated string");
            pos++;
        }
        p = malloc(pos - st + 1);
        memcpy(p, src + st, pos - st);
        p[pos - st] = 0;
        pos++;
        tok.s = p;
        tok.k = T_STR;
    } else {
        static const char *two[] = {":=", "<=", ">=", "<>", "..", 0};
        int i;
        tok.k = T_SYM;
        for (i = 0; two[i]; i++)
            if (!strncmp(src + pos, two[i], 2)) {
                tok.s = (char *)two[i];
                pos += 2;
                return;
            }
        if (strchr("+-*/=#<>&~(),;:.[]{}^|", src[pos])) {
            tok.s = fmt("%c", src[pos]);
            pos++;
        } else {
            die("illegal character '%c'", src[pos]);
        }
    }
}

int is(const char *s)
{
    return (tok.k == T_KW || tok.k == T_SYM) && !strcmp(tok.s, s);
}

int accept(const char *s)
{
    if (is(s)) {
        next();
        return 1;
    }
    return 0;
}

void expect(const char *s)
{
    if (!accept(s))
        die("'%s' expected, found '%s'", s,
            tok.k == T_EOF ? "end of file" : tok.s);
}

char *ident(void)
{
    char *s;
    if (tok.k != T_ID)
        die("identifier expected, found '%s'",
            tok.k == T_EOF ? "end of file" : tok.s);
    s = tok.s;
    next();
    return s;
}
