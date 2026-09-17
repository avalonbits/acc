/*
 * Names and tokens.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "ctype.h"

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

#ifdef ACC_HASH_STATS
/* Counts probes so a test can tell a hash that spreads from one that does
 * not -- both give correct answers, and only one of them is fast. Compiled
 * only when the test asks for it: in the compiler this would be an increment
 * on the hottest loop there is. */
unsigned long name_probes;
#define PROBE() (name_probes++)
#else
#define PROBE() ((void) 0)
#endif

/* Pearson hashing: one table lookup per character, and nothing else.
 *
 * FNV-1a was here, and on this target it was the most expensive thing in the
 * compiler. `h * 16777619` in a 32-bit accumulator is `call __lmulu`, and the
 * xor before it is `call __lxor` -- two calls into the long-arithmetic library
 * for every character of every identifier, including every `int` and every
 * `return`. The eZ80 has no multiplier wider than the 8-bit MLT and no 32-bit
 * anything, so there was no instruction for either half of it.
 *
 * Pearson replaces the arithmetic with `t[h ^ c]`, which is an 8-bit xor and
 * an indexed load from 256 bytes -- both real instructions. Two lanes, seeded
 * differently, give the 16 bits the bucket index needs; one lane's 8 would cap
 * the table at 256 names.
 *
 * The table is a permutation of 0..255, which is what makes the lanes mix at
 * all: any repeated value would collapse the two inputs that map to it. */
static const unsigned char pearson[256] = {
    203,  49,  73,  37, 250,  58, 130, 246,  54, 244,  89, 107,
    253, 227, 167,  57,   3, 231, 230,  80,   9, 235, 197, 113,
    251,  78,  22,  47, 193, 238, 176, 132, 109, 217, 225, 190,
     53, 236,  72, 120,  75, 242,  39, 205,  70,  20,  64,   5,
    166,  69,  15,  86, 100, 212, 177, 232,  41,  60,  23, 171,
      8, 108, 125,  82, 219, 165,  26,  28, 180,  25, 218,  74,
    209, 133, 102,  51, 207, 118,  92, 237, 153,  18, 211,  81,
     36, 127,  43, 175,  85,  52, 189, 131, 183,  17,  95, 192,
    213,  88,  16,   1, 110,  99, 119, 240, 186, 172, 170, 228,
    137, 162, 158, 145, 255, 214, 199,  65, 157,   0, 160, 245,
     91,  35, 181,  10,  59, 243, 194, 123, 134,  79, 249, 115,
    178,  34, 101, 144, 248, 239,  84, 141,  67, 185, 215, 140,
    184, 198, 149, 173, 187,  45, 223, 210, 117,  38, 161, 252,
     50, 229, 216, 139, 247,  98,  96,  90,  14,   2, 168, 196,
     42,  30,  56,  87, 201,  24, 142,   4,  33, 126, 148,  71,
     55, 159, 147,  13,  29, 234,  31, 179,  77, 150, 233, 114,
    200, 155, 104,  66, 224,  83,   6,  93,  12,  19,   7,  48,
    122, 182, 220, 254,  76, 152, 121,  46, 202, 206,  11, 124,
    191,  32, 103, 208, 135, 106, 226, 105, 204, 116, 163, 138,
     68, 111,  44,  63,  62, 136, 156, 143, 188, 128, 154, 146,
    169, 112, 151, 195, 221,  61, 129,  94,  27, 164, 222,  40,
    241,  97, 174,  21
};

/* Returns 16 bits. The caller masks it down to the table size. */
static unsigned name_hash(const char *text, int len)
{
    unsigned char high = 0;
    unsigned char low = (unsigned char) len;   /* so "ab" and "ba" differ */
    unsigned n = (unsigned) len, i;

    /* Unsigned, so the loop test is not a signed compare: `i < len` on two
     * ints is `call pe, __setflag` to repair the flags on overflow, and this
     * loop runs once per character of every identifier in the program. */
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char) text[i];

        high = pearson[high ^ c];
        low  = pearson[low ^ c];
    }

    /* One shift per name interned, not one per character. */
    return ((unsigned) high << 8) | low;
}

/* Split so that the test can be inlined into name_intern and the growth
 * cannot: the test is once per identifier, the growth is a dozen times in a
 * compile, and inlining both put a realloc's frame on the hot path. */
__attribute__((noinline))
static void names_realloc(size_t need)
{
    while (names_cap < names_len + need)
        names_cap = names_cap ? names_cap * 2 : 1024;
    names = realloc(names, names_cap);
    if (!names)
        acc_error("out of memory for names");
}

static void names_grow(size_t need)
{
    if (names_len + need > names_cap)
        names_realloc(need);
}

static void buckets_rehash(unsigned newn)
{
    NameRef *fresh = calloc(newn, sizeof *fresh);
    unsigned i;

    if (!fresh)
        acc_error("out of memory for the name table");
    for (i = 0; i < nbuckets; i++) {
        NameRef ref = buckets[i];
        unsigned slot;

        if (ref == NAME_NONE)
            continue;
        slot = name_hash(names + ref, (int) strlen(names + ref)) & (newn - 1);
        while (fresh[slot] != NAME_NONE)
            slot = (slot + 1) & (newn - 1);
        fresh[slot] = ref;
    }
    free(buckets);
    buckets = fresh;
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

NameRef name_intern(const char *text, int len)
{
    unsigned slot;
    NameRef ref;

    /* Kept under half full. Past that a linear probe starts walking. */
    if ((nnames + 1) * 2 >= nbuckets)
        buckets_rehash(nbuckets * 2);

    slot = name_hash(text, len) & (nbuckets - 1);
    PROBE();
    while ((ref = buckets[slot]) != NAME_NONE) {
        /* Compare, then check the terminator, rather than measure first.
         * strlen walks the stored name to its end before memcmp walks it
         * again, and it walked it even when the first character already said
         * the two were different. This runs 1.54 times per identifier in the
         * program. */
        if (memcmp(names + ref, text, len) == 0 && names[ref + len] == '\0')
            return ref;
        slot = (slot + 1) & (nbuckets - 1);
        PROBE();
    }

    names_grow(len + 1);
    ref = (NameRef) names_len;
    memcpy(names + names_len, text, len);
    names[names_len + len] = '\0';
    names_len += len + 1;
    buckets[slot] = ref;
    nnames++;

    return ref;
}

const char *name_text(NameRef ref)
{
    return names + ref;
}

/* ------------------------------------------------------------------ */
/* the source                                                          */

/* The whole file, read once. A four-hundred-line source is around 20 KB,
 * which is small beside the 206 KB a program has on the Agon -- the previous
 * compiler ran out on its symbol table, not on its input. Streaming it is a
 * change to make when there is a reason, not before. */
static char *src;
static char *cursor;
static const char *src_path;
static int   line;

int      tok;
int      tok_val;
NameRef  tok_name;
int      tok_line;
int      tok_prev_line;

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
    cursor = src;
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

static void skip_space(void)
{
    for (;;) {
        while (is_space(*cursor)) {
            if (*cursor == '\n')
                line++;
            cursor++;
        }
        if (cursor[0] == '/' && cursor[1] == '/') {
            while (*cursor && *cursor != '\n')
                cursor++;
            continue;
        }
        if (cursor[0] == '/' && cursor[1] == '*') {
            /* Where it opened, which is where the mistake is. Reporting the
             * line the scan gave up on points at the end of the file, which
             * is the one place the reader already knows is not the problem. */
            int opened = line;

            cursor += 2;
            while (*cursor && !(cursor[0] == '*' && cursor[1] == '/')) {
                if (*cursor == '\n')
                    line++;
                cursor++;
            }
            if (!*cursor)
                acc_error_at(opened, "unterminated comment");
            cursor += 2;
            continue;
        }
        return;
    }
}

/* The keywords, checked against an interned name rather than by strcmp on
 * every identifier. Interning gives each spelling one offset, so recognising
 * a keyword is three integer compares. */
/* Keywords are recognised by where they are, not by comparing against each
 * of them in turn.
 *
 * They are interned before anything else, so they occupy the first bytes of
 * the arena and every other name has a larger offset. That makes one compare
 * and one indexed load enough: a chain of `tok_name == kw_...` tests costs a
 * comparison per keyword on every identifier in the program, which is what
 * the compiler does most, and adding a keyword made it slower for everyone.
 * Adding one here costs nothing.
 *
 * The table has an entry per byte of arena the keywords occupy, of which only
 * the ones a name actually starts at are ever read. The rest say TK_IDENT so
 * that a bug reads as "not a keyword" rather than as whatever was there. */
static NameRef kw_limit;
static unsigned char kw_tok[64];

static void keyword(const char *text, int len, int token)
{
    NameRef ref = name_intern(text, len);

    if (ref >= (NameRef) sizeof kw_tok)
        acc_error("internal: the keyword table is too small");
    kw_tok[ref] = (unsigned char) token;
    if (ref + 1 > kw_limit)
        kw_limit = ref + 1;
}

static void keywords_init(void)
{
    unsigned i;

    for (i = 0; i < sizeof kw_tok; i++)
        kw_tok[i] = TK_IDENT;

    keyword("int", 3, TK_KW_INT);
    keyword("void", 4, TK_KW_VOID);
    keyword("return", 6, TK_KW_RETURN);
    keyword("if", 2, TK_KW_IF);
    keyword("else", 4, TK_KW_ELSE);
    keyword("while", 5, TK_KW_WHILE);
}

void next(void)
{
    int c;

    skip_space();
    tok_prev_line = tok_line;
    tok_line = line;
    c = (unsigned char) *cursor;

    if (c == '\0') {
        tok = TK_EOF;
        return;
    }

    if (is_digit(c)) {
        int v = 0;

        if (c == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
            cursor += 2;
            if (!is_alnum((unsigned char) *cursor))
                acc_error_at(line, "hex constant with no digits");
            while (is_alnum((unsigned char) *cursor)) {
                int d = *cursor;

                if (is_digit(d))          d -= '0';
                else if (d >= 'a' && d <= 'f') d -= 'a' - 10;
                else if (d >= 'A' && d <= 'F') d -= 'A' - 10;
                else acc_error_at(line, "bad digit '%c' in a hex constant", d);
                v = v * 16 + d;
                cursor++;
            }
        } else {
            while (is_digit((unsigned char) *cursor))
                v = v * 10 + (*cursor++ - '0');
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
        const char *s = cursor;

        while (is_alnum((unsigned char) *cursor))
            cursor++;
        tok_name = name_intern(s, (int) (cursor - s));
        tok = (tok_name < kw_limit) ? kw_tok[tok_name] : TK_IDENT;
        return;
    }

    cursor++;
    switch (c) {
    case '(': tok = TK_LPAREN;  return;
    case ')': tok = TK_RPAREN;  return;
    case '{': tok = TK_LBRACE;  return;
    case '}': tok = TK_RBRACE;  return;
    case ';': tok = TK_SEMI;    return;
    case ',': tok = TK_COMMA;   return;
    case '=':
        if (*cursor == '=') { cursor++; tok = TK_EQ; return; }
        tok = TK_ASSIGN;
        return;
    case '!':
        if (*cursor == '=') { cursor++; tok = TK_NE; return; }
        tok = TK_NOT;
        return;
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
        if (*cursor == '<') { cursor++; tok = TK_SHL; return; }
        if (*cursor == '=') { cursor++; tok = TK_LE;  return; }
        tok = TK_LT;
        return;
    case '>':
        if (*cursor == '>') { cursor++; tok = TK_SHR; return; }
        if (*cursor == '=') { cursor++; tok = TK_GE;  return; }
        tok = TK_GT;
        return;
    }

    acc_error_at(line, "stray '%c' in the source", c);
}

const char *tok_spelling(int token)
{
    switch (token) {
    case TK_EOF:       return "end of file";
    case TK_INT:       return "a number";
    case TK_IDENT:     return "a name";
    case TK_KW_INT:    return "'int'";
    case TK_KW_VOID:   return "'void'";
    case TK_KW_RETURN: return "'return'";
    case TK_KW_IF:     return "'if'";
    case TK_KW_ELSE:   return "'else'";
    case TK_KW_WHILE:  return "'while'";
    case TK_LPAREN:    return "'('";
    case TK_RPAREN:    return "')'";
    case TK_LBRACE:    return "'{'";
    case TK_RBRACE:    return "'}'";
    case TK_SEMI:      return "';'";
    case TK_COMMA:     return "','";
    case TK_ASSIGN:    return "'='";
    case TK_PLUS:      return "'+'";
    case TK_MINUS:     return "'-'";
    case TK_STAR:      return "'*'";
    case TK_SLASH:     return "'/'";
    case TK_PERCENT:   return "'%'";
    case TK_AMP:       return "'&'";
    case TK_PIPE:      return "'|'";
    case TK_CARET:     return "'^'";
    case TK_TILDE:     return "'~'";
    case TK_SHL:       return "'<<'";
    case TK_SHR:       return "'>>'";
    case TK_LT:        return "'<'";
    case TK_GT:        return "'>'";
    case TK_LE:        return "'<='";
    case TK_GE:        return "'>='";
    case TK_EQ:        return "'=='";
    case TK_NE:        return "'!='";
    case TK_NOT:       return "'!'";
    }

    return "that";
}

/* The operators the lexer knows and the code generator does not, so that the
 * parser can name the missing feature rather than complain about a ';'. The
 * eZ80 has no instruction for any of them; they are the next milestone. */
int tok_is_unimplemented_op(int token)
{
    switch (token) {
    case TK_STAR: case TK_SLASH: case TK_PERCENT:
    case TK_AMP:  case TK_PIPE:  case TK_CARET:
    case TK_SHL:  case TK_SHR:
    case TK_NOT:
        return 1;
    }

    return 0;
}

int accept(int token)
{
    if (tok != token)
        return 0;
    next();

    return 1;
}

void expect(int token, const char *what)
{
    if (tok != token) {
        /* An operator acc has not got to yet turns up where a statement was
         * meant to end. Say which operator rather than ask for the ';'. */
        if (tok_is_unimplemented_op(tok))
            acc_error_at(tok_line, "%s is not supported yet", tok_spelling(tok));

        /* Otherwise point at where the missing token should have gone, which
         * is the end of the token before it, not the start of whatever
         * turned up instead: a ';' left off the end of line 4 is a mistake
         * on line 4, even though the '}' that reveals it is on line 5. */
        acc_error_at(tok_prev_line, "expected %s, found %s", what, tok_spelling(tok));
    }
    next();
}

void lex_init(void)
{
    keywords_init();
}
