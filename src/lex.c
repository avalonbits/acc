/*
 * Names and tokens.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"

/* ------------------------------------------------------------------ */
/* the name arena                                                      */

/* One block of characters holding every identifier in the program, each one
 * exactly once, NUL terminated. Identifiers are referred to by their offset
 * into it, so a name costs three bytes wherever it is mentioned.
 *
 * Nothing is ever freed. A compiler runs once and exits; a free list would be
 * bytes spent to give memory back to a process that is about to end.
 *
 * Offset zero is never a name, so it can mean "none". */
static char  *names;
static size_t names_len, names_cap;

/* Open addressing over the arena: each slot is an offset, and a collision
 * walks forward. Sized as a power of two so the modulo is a mask, and grown by
 * doubling so the transient peak stays near 1.5x rather than 2x -- the
 * difference matters on a machine with no virtual memory. */
static NameRef *buckets;
static unsigned nbuckets, nnames;

static unsigned name_hash(const char *s, int len)
{
    unsigned h = 2166136261u;
    int i;

    for (i = 0; i < len; i++)
        h = (h ^ (unsigned char) s[i]) * 16777619u;

    return h;
}

static void names_grow(size_t need)
{
    if (names_len + need <= names_cap)
        return;
    while (names_cap < names_len + need)
        names_cap = names_cap ? names_cap * 2 : 1024;
    names = realloc(names, names_cap);
    if (!names)
        acc_error("out of memory for names");
}

static void buckets_rehash(unsigned newn)
{
    NameRef *nb = calloc(newn, sizeof *nb);
    unsigned i;

    if (!nb)
        acc_error("out of memory for the name table");
    for (i = 0; i < nbuckets; i++) {
        NameRef r = buckets[i];
        unsigned h;

        if (r == NAME_NONE)
            continue;
        h = name_hash(names + r, (int) strlen(names + r)) & (newn - 1);
        while (nb[h] != NAME_NONE)
            h = (h + 1) & (newn - 1);
        nb[h] = r;
    }
    free(buckets);
    buckets = nb;
    nbuckets = newn;
}

void name_init(void)
{
    names_cap = 1024;
    names = malloc(names_cap);
    if (!names)
        acc_error("out of memory for names");
    names[0] = '\0';      /* so offset 0 is never a real name */
    names_len = 1;
    buckets_rehash(256);
}

NameRef name_intern(const char *s, int len)
{
    unsigned h;
    NameRef r;

    /* Kept under half full. Past that a linear probe starts walking. */
    if ((nnames + 1) * 2 >= nbuckets)
        buckets_rehash(nbuckets * 2);

    h = name_hash(s, len) & (nbuckets - 1);
    while ((r = buckets[h]) != NAME_NONE) {
        if ((int) strlen(names + r) == len && memcmp(names + r, s, len) == 0)
            return r;
        h = (h + 1) & (nbuckets - 1);
    }

    names_grow(len + 1);
    r = (NameRef) names_len;
    memcpy(names + names_len, s, len);
    names[names_len + len] = '\0';
    names_len += len + 1;
    buckets[h] = r;
    nnames++;

    return r;
}

const char *name_text(NameRef n)
{
    return names + n;
}

/* ------------------------------------------------------------------ */
/* the source                                                          */

/* The whole file, read once. A four-hundred-line source is around 20 KB,
 * which is small beside the 206 KB a program has on the Agon -- the previous
 * compiler ran out on its symbol table, not on its input. Streaming it is a
 * change to make when there is a reason, not before. */
static char *src;
static char *p;
static const char *src_path;
static int   line;

int      tok;
int      tok_val;
NameRef  tok_name;
int      tok_line;

void lex_open(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;

    if (!f)
        acc_error("cannot open '%s'", path);
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    src = malloc(n + 1);
    if (!src)
        acc_error("out of memory reading '%s'", path);
    if ((long) fread(src, 1, n, f) != n)
        acc_error("short read on '%s'", path);
    src[n] = '\0';
    fclose(f);

    src_path = path;
    p = src;
    line = 1;
    next();
}

void lex_close(void)
{
    free(src);
    src = NULL;
}

const char *lex_path(void) { return src_path; }
int lex_line(void)         { return line; }

static int is_space(int c)  { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static int is_digit(int c)  { return c >= '0' && c <= '9'; }
static int is_alpha(int c)  { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int is_alnum(int c)  { return is_alpha(c) || is_digit(c); }

static void skip_space(void)
{
    for (;;) {
        while (is_space(*p)) {
            if (*p == '\n')
                line++;
            p++;
        }
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n')
                p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) {
                if (*p == '\n')
                    line++;
                p++;
            }
            if (!*p)
                acc_error_at(line, "unterminated comment");
            p += 2;
            continue;
        }
        return;
    }
}

/* The keywords, checked against an interned name rather than by strcmp on
 * every identifier. Interning gives each spelling one offset, so recognising
 * a keyword is three integer compares. */
static NameRef kw_int, kw_void, kw_return;

static void keywords_init(void)
{
    kw_int    = name_intern("int", 3);
    kw_void   = name_intern("void", 4);
    kw_return = name_intern("return", 6);
}

void next(void)
{
    int c;

    skip_space();
    tok_line = line;
    c = (unsigned char) *p;

    if (c == '\0') {
        tok = TK_EOF;
        return;
    }

    if (is_digit(c)) {
        int v = 0;

        if (c == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
            if (!is_alnum((unsigned char) *p))
                acc_error_at(line, "hex constant with no digits");
            while (is_alnum((unsigned char) *p)) {
                int d = *p;

                if (is_digit(d))          d -= '0';
                else if (d >= 'a' && d <= 'f') d -= 'a' - 10;
                else if (d >= 'A' && d <= 'F') d -= 'A' - 10;
                else acc_error_at(line, "bad digit '%c' in a hex constant", d);
                v = v * 16 + d;
                p++;
            }
        } else {
            while (is_digit((unsigned char) *p))
                v = v * 10 + (*p++ - '0');
        }
        /* Narrowed to what the target can hold, so that a literal on the
         * value stack is the number the machine would have. */
        v &= 0xffffff;
        if (v & 0x800000)
            v -= 0x1000000;
        tok = TK_INT;
        tok_val = v;
        return;
    }

    if (is_alpha(c)) {
        const char *s = p;

        while (is_alnum((unsigned char) *p))
            p++;
        tok_name = name_intern(s, (int) (p - s));
        if (tok_name == kw_int)         tok = TK_KW_INT;
        else if (tok_name == kw_void)   tok = TK_KW_VOID;
        else if (tok_name == kw_return) tok = TK_KW_RETURN;
        else                            tok = TK_IDENT;
        return;
    }

    p++;
    switch (c) {
    case '(': tok = TK_LPAREN;  return;
    case ')': tok = TK_RPAREN;  return;
    case '{': tok = TK_LBRACE;  return;
    case '}': tok = TK_RBRACE;  return;
    case ';': tok = TK_SEMI;    return;
    case ',': tok = TK_COMMA;   return;
    case '=': tok = TK_ASSIGN;  return;
    case '+': tok = TK_PLUS;    return;
    case '-': tok = TK_MINUS;   return;
    case '*': tok = TK_STAR;    return;
    case '/': tok = TK_SLASH;   return;
    case '%': tok = TK_PERCENT; return;
    case '&': tok = TK_AMP;     return;
    case '|': tok = TK_PIPE;    return;
    case '^': tok = TK_CARET;   return;
    case '~': tok = TK_TILDE;   return;
    case '<':
        if (*p == '<') { p++; tok = TK_SHL; return; }
        break;
    case '>':
        if (*p == '>') { p++; tok = TK_SHR; return; }
        break;
    }

    acc_error_at(line, "stray '%c' in the source", c);
}

const char *tok_spelling(int t)
{
    switch (t) {
    case TK_EOF:       return "end of file";
    case TK_INT:       return "a number";
    case TK_IDENT:     return "a name";
    case TK_KW_INT:    return "'int'";
    case TK_KW_VOID:   return "'void'";
    case TK_KW_RETURN: return "'return'";
    case TK_LPAREN:    return "'('";
    case TK_RPAREN:    return "')'";
    case TK_LBRACE:    return "'{'";
    case TK_RBRACE:    return "'}'";
    case TK_SEMI:      return "';'";
    case TK_COMMA:     return "','";
    case TK_ASSIGN:    return "'='";
    }

    return "that";
}

int accept(int t)
{
    if (tok != t)
        return 0;
    next();

    return 1;
}

void expect(int t, const char *what)
{
    if (tok != t)
        acc_error_at(tok_line, "expected %s, found %s", what, tok_spelling(tok));
    next();
}

void lex_init(void)
{
    keywords_init();
}
