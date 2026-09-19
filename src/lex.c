/*
 * Names and tokens.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "ctype.h"

/* ------------------------------------------------------------------ */
/* the name arena                                                      */

/* One block of characters holding every identifier in the program, each one
 * exactly once, NUL terminated. Identifiers are referred to by their offset
 * into it, so a name costs three bytes wherever it is mentioned. The three
 * bytes in front of each name's text hold the file-scope symbol it stands
 * for: see name_global.
 *
 * Nothing is ever freed. A compiler runs once and exits; a free list would be
 * bytes spent to give memory back to a process that is about to end.
 *
 * Offset zero is never a name, so it can mean "none". */
char         *name_arena;       /* name_global reads it directly */
static size_t names_len, names_cap;

/* Open addressing over the arena: each slot is an offset, and a collision
 * walks forward. Sized as a power of two so the modulo is a mask, and grown by
 * doubling so the transient peak stays near 1.5x rather than 2x -- the
 * difference matters on a machine with no virtual memory. */
/* A slot is four bytes and not three, so that a slot's offset is a byte
 * pattern that can be assembled without arithmetic: see home, below. A
 * three-byte slot would make it a multiply, `call __imulu`. The probe runs for
 * every identifier in the program, which is what the compiler does most.
 *
 * An empty slot holds NAME_NONE. */
typedef union {
    NameRef       ref;
    unsigned char size[4];      /* four bytes on the host too, so the hash
                                 * test measures the table the Agon has */
} Bucket;

static Bucket  *buckets, *buckets_end;
static unsigned nbuckets, nnames;
static unsigned names_room;     /* names that fit before the table must grow */

/* Where a name's probe starts, as a byte offset into the table: a slot
 * number times the slot's size, masked to the table's size. It is assembled a byte at a
 * time, in memory, because each way of computing it in a register is a helper
 * call -- `high << 8` is `call __ishl`, masking a 24-bit value is `call
 * __iand`, and the slot times four is `call __ishl` again -- and those three
 * ran for every identifier in the program. Byte-wise they are an 8-bit `and`
 * and a store each, and the load that reads the offset back is one 24-bit
 * load. It has to go through memory: assembled in a local, the compiler
 * recognises the pattern and puts the shifts back.
 *
 * The bytes above the second stay zero, which caps the table at 64 KB: 16384
 * slots, and so 8191 distinct names in a program on the Agon. */
static unsigned char home[sizeof(unsigned)];
static unsigned char mask_lo, mask_hi;

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

/* Returns the bucket a name's probe starts at. */
static inline __attribute__((always_inline))
Bucket *name_home(const char *text, unsigned n)
{
    /* Sixteen bits from one table lookup per character rather than two.
     *
     * Two independent Pearson lanes is the obvious way to widen the hash and
     * it costs a lookup per character per lane. Only one lane has to be
     * that good. The other only has to carry what the first loses, and
     * doubling and adding the character does that for two adds against a
     * second indexed load; one more lookup at the end, once per name, mixes
     * it in.
     *
     * Cheaper and better, which was not the expected result. Measured over
     * the two thousand names the hash test interns, the two-lane version
     * needed 1.54 probes for a lookup and this one 1.46 when the table took
     * its index from the low sixteen bits -- the shift carries position
     * information that a second Pearson lane, being the same function of the
     * same bytes, largely repeats.
     */
    unsigned char low = (unsigned char) n;     /* so "ab" and "ba" differ */
    unsigned char high = 0;
    unsigned i, offset;

    /* Unsigned, so the loop test is not a signed compare: `i < len` on two
     * ints is `call pe, __setflag` to repair the flags on overflow, and this
     * loop runs once per character of every identifier in the program. */
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char) text[i];

        low  = pearson[low ^ c];
        high = (unsigned char) (high + high + c);
    }

    /* The offset's low byte is a multiple of the slot's size, so whatever
     * goes there loses its bottom two bits, and neither byte can be `high`
     * as it stands: its low bits are the last character's and not much else.
     * Both bytes are therefore mixed ones, and together they still tell
     * apart every pair of bytes the loop could have ended on. Over the hash
     * test's names this probes 1.50 times a lookup; `low` and `high` as
     * they were, low byte first, probe 1.96, and a uniform hash at this load
     * 1.48. */
    high = (unsigned char) (pearson[high] ^ low);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    offset = ((unsigned) (low & mask_hi) << 8) | (high & mask_lo);
#else
    home[0] = high & mask_lo;
    home[1] = low & mask_hi;
    memcpy(&offset, home, sizeof offset);
#endif

    return (Bucket *) ((char *) buckets + offset);
}

/* Split so that the test can be inlined into name_intern and the growth
 * cannot: the test is once per identifier, the growth is a dozen times in a
 * compile, and inlining both put a realloc's frame on the hot path. */
__attribute__((noinline))
static void names_realloc(size_t need)
{
    while (names_cap < names_len + need)
        names_cap = names_cap ? names_cap * 2 : 1024;
    name_arena = realloc(name_arena, names_cap);
    if (!name_arena)
        acc_error("out of memory for names");
}

static void names_grow(size_t need)
{
    if (names_len + need > names_cap)
        names_realloc(need);
}

static void buckets_rehash(unsigned newn)
{
    Bucket *old = buckets, *end = buckets_end, *from;
    unsigned mask = newn * sizeof *buckets - 1;

    if (newn * sizeof *buckets > 65536)
        acc_error("too many names: the limit is %u", 65536 / sizeof *buckets / 2 - 1);
    buckets = calloc(newn, sizeof *buckets);
    if (!buckets)
        acc_error("out of memory for the name table");
    buckets_end = buckets + newn;
    nbuckets = newn;
    names_room = newn / 2 - 1 - nnames;
    mask_lo = (unsigned char) (mask & ~(sizeof *buckets - 1));
    mask_hi = (unsigned char) (mask >> 8);
    for (from = old; from != end; from++) {
        NameRef ref = from->ref;
        Bucket *b;

        if (ref == NAME_NONE)
            continue;
        b = name_home(name_arena + ref, (unsigned) strlen(name_arena + ref));
        while (b->ref != NAME_NONE)
            if (++b == buckets_end)
                b = buckets;
        b->ref = ref;
    }
    free(old);
}

void name_init(void)
{
    names_cap = 1024;
    name_arena = malloc(names_cap);
    if (!name_arena)
        acc_error("out of memory for names");
    name_arena[0] = '\0';     /* so offset 0 is never a real name */
    names_len = 1;
    buckets_rehash(256);
}

NameRef name_intern(const char *text, int len)
{
    Bucket *b = name_home(text, (unsigned) len);
    NameRef ref;

    PROBE();
    while ((ref = b->ref) != NAME_NONE) {
        /* Compare, then check the terminator, rather than measure first.
         * strlen walks the stored name to its end before the compare walks it
         * again, and it walked it even when the first character already said
         * the two were different. This runs 1.5 times per identifier in the
         * program.
         *
         * strncmp and not memcmp, which reads all len bytes of the stored
         * name whether or not it ends first -- off the end of the arena when
         * it is the last name there. strncmp stops at its terminator, so the
         * check after it only runs on a name at least len long. */
        if (strncmp(name_arena + ref, text, len) == 0
            && name_arena[ref + len] == '\0')
            return ref;
        if (++b == buckets_end)
            b = buckets;
        PROBE();
    }

    /* Kept under half full, since past that a linear probe starts walking.
     * Checked here, where the name is new, rather than on every lookup: the
     * growth moves every name, so the probe starts again. */
    if (names_room == 0) {
        buckets_rehash(nbuckets * 2);

        return name_intern(text, len);
    }

    /* No file-scope symbol yet, then the text. */
    names_grow(3 + len + 1);
    name_arena[names_len] = name_arena[names_len + 1] = name_arena[names_len + 2] = 0;
    ref = (NameRef) names_len + 3;
    memcpy(name_arena + ref, text, len);
    name_arena[ref + len] = '\0';
    names_len = ref + len + 1;
    b->ref = ref;
    nnames++;
    names_room--;

    return ref;
}

const char *name_text(NameRef ref)
{
    return name_arena + ref;
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
long     tok_val;
float    tok_fval;
typedef char float_is_four_bytes[sizeof(float) == 4 ? 1 : -1];
NameRef  tok_name;
int      tok_line;
Type     tok_type;
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

/* Whether the next character that is not white space is a colon. That is all
 * that tells a label from an expression statement -- `done:` and `done = 1;`
 * start with the same token -- and one character is enough to look at: at
 * the start of a statement a name followed by a colon can only be a label.
 * Nothing is consumed. */
int lex_colon_follows(void)
{
    const char *p = cursor;

    while (is_space(*p))
        p++;

    return *p == ':';
}

/* A comment, with the cursor on its opening `/`. Out of line so that
 * skip_space, which is inlined into next(), carries nothing but the cursor
 * and the line count: the comment scan's own state, the line a block comment
 * opened on, was enough to give next() a stack frame -- on every token,
 * where comments are one call each. */
__attribute__((noinline))
static void skip_comment(void)
{
    if (cursor[1] == '/') {
        while (*cursor && *cursor != '\n')
            cursor++;

        return;
    }

    /* Where it opened, which is where the mistake is. Reporting the line the
     * scan gave up on points at the end of the file, which is the one place
     * the reader already knows is not the problem. */
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
}

__attribute__((noinline))
static void skip_space(void)
{
    for (;;) {
        while (is_space(*cursor)) {
            if (*cursor == '\n')
                line++;
            cursor++;
        }
        if (cursor[0] != '/' || (cursor[1] != '/' && cursor[1] != '*'))
            return;
        skip_comment();
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
 * Each keyword's token is kept where any other name keeps its file-scope
 * symbol, in the bytes in front of its text: a keyword can never name one,
 * so the field is free, and the lexer reads the token from the name it has
 * just interned. */
static NameRef kw_limit;

static void keyword(const char *text, int len, int token)
{
    NameRef ref = name_intern(text, len);

    name_arena[ref - 3] = (char) token;
    kw_limit = ref + 1;
}

static void keywords_init(void)
{
    keyword("int", 3, TK_KW_INT);
    keyword("void", 4, TK_KW_VOID);
    keyword("return", 6, TK_KW_RETURN);
    keyword("if", 2, TK_KW_IF);
    keyword("else", 4, TK_KW_ELSE);
    keyword("while", 5, TK_KW_WHILE);
    keyword("char", 4, TK_KW_CHAR);
    keyword("short", 5, TK_KW_SHORT);
    keyword("long", 4, TK_KW_LONG);
    keyword("signed", 6, TK_KW_SIGNED);
    keyword("unsigned", 8, TK_KW_UNSIGNED);
    keyword("float", 5, TK_KW_FLOAT);
    keyword("double", 6, TK_KW_DOUBLE);
    keyword("for", 3, TK_KW_FOR);

    /* The rest of what C99 reserves. None of it is implemented and all of it
     * is refused by name, which is the whole reason for interning it: a word
     * the lexer does not know becomes an identifier, and the complaint that
     * follows is about a name not being declared rather than about the
     * feature not being there. */
    keyword("auto", 4, TK_KW_RESERVED);
    keyword("break", 5, TK_KW_BREAK);
    keyword("case", 4, TK_KW_CASE);
    keyword("const", 5, TK_KW_RESERVED);
    keyword("continue", 8, TK_KW_CONTINUE);
    keyword("default", 7, TK_KW_DEFAULT);
    keyword("do", 2, TK_KW_DO);
    keyword("enum", 4, TK_KW_RESERVED);
    keyword("extern", 6, TK_KW_RESERVED);
    keyword("goto", 4, TK_KW_GOTO);
    keyword("inline", 6, TK_KW_RESERVED);
    keyword("register", 8, TK_KW_RESERVED);
    keyword("restrict", 8, TK_KW_RESERVED);
    keyword("sizeof", 6, TK_KW_SIZEOF);
    keyword("static", 6, TK_KW_RESERVED);
    keyword("struct", 6, TK_KW_RESERVED);
    keyword("switch", 6, TK_KW_SWITCH);
    keyword("typedef", 7, TK_KW_RESERVED);
    keyword("union", 5, TK_KW_RESERVED);
    keyword("volatile", 8, TK_KW_RESERVED);
    keyword("_Bool", 5, TK_KW_RESERVED);
    keyword("_Complex", 8, TK_KW_RESERVED);
    keyword("_Imaginary", 10, TK_KW_RESERVED);
}

/* Punctuation, by the character that begins it. TK_EOF means the character
 * is not punctuation at all, which is what makes zero the right default.
 *
 * A switch over these compiled to twenty-three comparisons in a row -- the
 * backend builds no jump table -- so a `;` cost a few and anything late in
 * the list cost twenty, once per punctuation token in the program. */
static const unsigned char punct[256] = {
    ['('] = TK_LPAREN, [')'] = TK_RPAREN,
    ['{'] = TK_LBRACE, ['}'] = TK_RBRACE,
    [';'] = TK_SEMI,   [','] = TK_COMMA,
    ['='] = TK_ASSIGN, ['!'] = TK_NOT,
    ['+'] = TK_PLUS,   ['-'] = TK_MINUS,
    ['*'] = TK_STAR,   ['/'] = TK_SLASH,  ['%'] = TK_PERCENT,
    ['&'] = TK_AMP,    ['|'] = TK_PIPE,   ['^'] = TK_CARET,
    ['~'] = TK_TILDE,
    ['<'] = TK_LT,     ['>'] = TK_GT,
    ['?'] = TK_QUESTION, [':'] = TK_COLON,
    ['['] = TK_LBRACKET, [']'] = TK_RBRACKET
};

/* Read a floating literal from the cursor, which is at its first character.
 *
 * Converted by float_literal rather than the C library's strtod, which on the
 * Agon does not round correctly and on the host rounds twice: see float.c.
 *
 * Out of line because next() is the hottest function in the compiler and most
 * programs have no floating literals at all. */
__attribute__((noinline))
static void lex_floating(void)
{
    uint32_t bits;
    const char *end = float_literal(cursor, &bits);

    if (end == cursor)
        acc_error_at(tok_line, "a floating-point number with no digits");
    memcpy(&tok_fval, &bits, sizeof tok_fval);
    cursor = (char *) end;
    if (*cursor == 'f' || *cursor == 'F')
        cursor++;
    tok = TK_FLOAT;
    tok_type = TY_FLOAT;
}

/* Out of line, and the attribute is load-bearing -- there is one caller, so
 * it goes straight back inline without it.
 *
 * It is not about the size of the code. `value` is an unsigned long, which is
 * four bytes on a three-byte machine, and the accumulator and the bound it is
 * checked against want frame slots. Inline, next() opened a frame wide enough
 * for them on every token it read, and the tokens a program is mostly made of
 * are names and punctuation, which need no frame at all. Out of line, the
 * cost is paid by the numeric constants and by nothing else. */
__attribute__((noinline))
static void lex_number(void)
{
    /* One or two decimal digits, with nothing after them that could make the
     * constant anything but a small int -- no suffix, no point, no exponent,
     * and no leading 0 that would make it octal or hex. That is most of the
     * constants in a program, and the answer is at most 99, so the multiply
     * is a byte one: this target does it with MLT and no call into the
     * runtime, where the general path below makes four such calls for every
     * digit and then climbs a ladder of four-byte compares. A wider fast path
     * in an int was tried and was slower -- a 24-bit multiply is itself a
     * call, and the accumulator lived in the frame. */
    {
        unsigned char first = (unsigned char) cursor[0];
        unsigned char second = (unsigned char) cursor[1];

        if (!is_alnum(second) && second != '.') {
            cursor++;
            tok = TK_INT;
            tok_type = TY_INT;
            tok_val = first - '0';

            return;
        }
        if (first != '0' && is_digit(second)) {
            unsigned char third = (unsigned char) cursor[2];

            if (!is_alnum(third) && third != '.') {
                cursor += 2;
                tok = TK_INT;
                tok_type = TY_INT;
                tok_val = (unsigned char) ((first - '0') * 10 + (second - '0'));

                return;
            }
        }
    }

    /* Accumulated with the bound checked before each step rather than
     * after, so the accumulator never overflows and there is nothing to
     * detect after the fact. Everything here stays in an int, which on
     * this target is the 24 bits the answer has to fit in anyway. */
    /* Unsigned, because the largest constant acc takes is 0xFFFFFFFF and
     * long on this target is 32 bits signed. The bits are what is wanted;
     * tok_val holds them and the type says how to read them.
     *
     * Exactly 32 bits, and not `unsigned long`, which is 32 bits when acc is
     * compiled for the Agon and 64 when it is compiled for the host it is
     * tested on. With the guard below reading a width that changes, the two
     * builds disagreed about which constants a program may contain: the host
     * build accumulated an eleven-digit constant without complaint and let
     * the ladder decide, while the Agon build wrapped and refused it. The
     * width the answer has to fit in is a fact about the language acc
     * compiles, not about the machine acc is running on. */
    uint32_t value = 0;
    int not_decimal = 0;        /* hex or octal: may be typed unsigned */
    int bad_digit = 0;
    int overflowed = 0;
    int suffix_u = 0, suffix_l = 0;
    char *start = cursor;

    tok_type = TY_VOID;         /* no type chosen yet; the ladder picks one */

    if (*cursor == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
        not_decimal = 1;
        cursor += 2;
        if (!is_alnum((unsigned char) *cursor))
            acc_error_at(tok_line, "hex constant with no digits");
        while (is_alnum((unsigned char) *cursor)) {
            int digit = *cursor;

            if (is_digit(digit))                     digit -= '0';
            else if (digit >= 'a' && digit <= 'f')   digit -= 'a' - 10;
            else if (digit >= 'A' && digit <= 'F')   digit -= 'A' - 10;
            else acc_error_at(tok_line, "bad digit '%c' in a hex constant", digit);
            if (value > 0xfffffffUL)
                acc_error_at(tok_line, "the constant does not fit in %d bits",
                             ACC_LONG_SIZE * 8);
            value = value * 16 + digit;
            cursor++;
        }
    } else {
        /* A leading 0 followed by more digits is octal, which C has always
         * said and acc did not: `010` was ten. An 8 or a 9 in one is noted
         * rather than refused, for the same reason the overflow below is --
         * `09.5` is an ordinary decimal float, and which this is shows only
         * at the character the digits stop at. */
        int octal = (*cursor == '0' && is_digit((unsigned char) cursor[1]));
        int bad_octal = 0;

        not_decimal = octal;

        /* The first digits in an unsigned, for as long as another digit
         * cannot carry it past 24 bits: that covers every decimal constant
         * below ten million, and the steps are a 24-bit multiply and an add
         * where the loop below makes a four-byte multiply, add and compare
         * of each digit. Octal is left to the loop: it is rare, and
         * its digits are fewer. */
        if (!octal) {
            unsigned small = 0;

            while (is_digit((unsigned char) *cursor) && small < 1677721u) {
                small = small * 10 + (unsigned) (*cursor - '0');
                cursor++;
            }
            value = small;
        }
        while (is_digit((unsigned char) *cursor)) {
            int digit = *cursor - '0';

            /* Noted rather than refused, because the digits might still turn
             * out to be the whole part of a floating literal, where a
             * hundred of them are ordinary. Only once the terminator says
             * this was an integer does too many digits become an error. */
            if (octal) {
                if (digit > 7 && !bad_octal)
                    bad_octal = *cursor;
                if (value > 0x1fffffffUL)
                    overflowed = 1;
                else
                    value = value * 8 + digit;
                cursor++;

                continue;
            }
            if (value > 429496729UL
                || (value == 429496729UL && digit > 5)) {
                overflowed = 1;
                cursor++;

                continue;
            }
            value = value * 10 + digit;
            cursor++;
        }
        bad_digit = bad_octal;
    }

    /* C99 types a constant by the first type that can hold it. A decimal
     * one goes int, long int, long long int; a hex or octal one may also
     * be unsigned at each step, which is the only place the two forms
     * differ. acc has no long long, so past the end of long it refuses. */
    /* The character the digits stopped at decides which kind it was. */
    if (*cursor == '.' || *cursor == 'e' || *cursor == 'E'
        || *cursor == 'f' || *cursor == 'F') {
        cursor = start;
        lex_floating();

        return;
    }

    if (bad_digit)
        acc_error_at(tok_line, "'%c' is not an octal digit, and a constant that "
                           "starts with 0 is octal", bad_digit);
    if (overflowed)
        acc_error_at(tok_line, "the constant does not fit in %d bits",
                     ACC_LONG_SIZE * 8);

    /* The suffix, which narrows the list before the value is measured
     * against it: u takes the signed types out, l takes int out. Either
     * order and either case, and at most one u and two l -- and `lL` is not
     * a suffix at all, because the two letters of a long long one have to
     * match. */
    while (*cursor == 'u' || *cursor == 'U'
           || *cursor == 'l' || *cursor == 'L') {
        if (*cursor == 'u' || *cursor == 'U') {
            if (suffix_u)
                acc_error_at(tok_line, "the constant has more than one 'u' suffix");
            suffix_u = 1;
            cursor++;

            continue;
        }

        if (suffix_l)
            acc_error_at(tok_line, "the constant has more than one 'l' suffix");
        if (cursor[1] == *cursor) {
            acc_error_at(tok_line, "'long long' is not supported yet");
        }
        suffix_l = 1;
        cursor++;
    }
    if (is_alnum((unsigned char) *cursor))
        acc_error_at(tok_line, "'%c' is not a suffix a constant can have", *cursor);

    /* C99 types a constant by the first type in that list that can hold it.
     * The unsigned types are in it when the suffix says u, and also when a
     * hex or octal constant has no suffix at all -- which is the only place
     * the decimal and hex forms differ. acc has no long long, so past the end
     * of long it refuses. */
    if (suffix_u) {
        if (!suffix_l && value <= 0xffffffUL)
            tok_type = TY_UINT;
        else if (value <= 0xffffffffUL)
            tok_type = TY_ULONG;
    } else if (suffix_l) {
        if (value <= 0x7fffffffUL)
            tok_type = TY_LONG;
        else if (not_decimal)
            tok_type = TY_ULONG;
    } else if (value <= 0x7fffffUL) {
        tok_type = TY_INT;
    } else if (not_decimal && value <= 0xffffffUL) {
        tok_type = TY_UINT;
    } else if (value <= 0x7fffffffUL) {
        tok_type = TY_LONG;
    } else if (not_decimal) {
        tok_type = TY_ULONG;
    }

    if (tok_type == TY_VOID)
        acc_error_at(tok_line, "the constant is too large for a long, and "
                           "long long is not supported yet");

    /* An unsigned int is normalised to the signed pattern of the same 24
     * bits, so that acc folds it identically whether it is itself running on
     * a 24-bit int or a 32-bit one. */
    if (tok_type == TY_UINT && value > 0x7fffffUL)
        value -= 0x1000000UL;

    tok = TK_INT;
    tok_val = (long) value;
}

/* The punctuation next() refuses: a character that begins no token, or the
 * `&&=` and `||=` that C does not have, named by their first character. Out
 * of line, reading the line for itself, so that next() keeps nothing for an
 * error it will almost never report: holding the line and the character for
 * these calls was a stack frame on every token. */
__attribute__((noinline, noreturn))
static void punct_error(int c)
{
    if (c == '&' && cursor[0] == '=')
        acc_error_at(tok_line, "'&&=' is not an operator in C; "
                               "`a = a && b` is what it would mean");
    if (c == '|' && cursor[0] == '=')
        acc_error_at(tok_line, "'||=' is not an operator in C; "
                               "`a = a || b` is what it would mean");
    acc_error_at(tok_line, "stray '%c' in the source", c);
}

/* ------------------------------------------------------------------ */
/* character and string literals                                       */

const char *tok_str;
int         tok_str_len;

static char *str_buf;
static int   str_cap;

#define is_hexdigit(c) (is_digit(c) || ((unsigned) (((c) | 0x20) - 'a') < 6u))

/* One character of a literal, the backslash of an escape read already: the
 * byte it stands for. Octal takes up to three digits and hex as many as
 * there are, which is what C says; a value past a byte is refused. */
static int escape(void)
{
    int c = (unsigned char) *cursor++, value, digits;

    switch (c) {
    case 'n':  return '\n';
    case 't':  return '\t';
    case 'r':  return '\r';
    case 'a':  return 7;
    case 'b':  return 8;
    case 'f':  return 12;
    case 'v':  return 11;
    case '\\': case '\'': case '"': case '?':
        return c;
    case 'x':
        if (!is_hexdigit((unsigned char) *cursor))
            acc_error_at(tok_line, "'\\x' needs a hex digit after it");
        value = 0;
        while (is_hexdigit((unsigned char) *cursor)) {
            int d = *cursor++;

            d = is_digit(d) ? d - '0' : (d | 0x20) - 'a' + 10;
            value = value * 16 + d;
            if (value > 255)
                acc_error_at(tok_line, "a '\\x' escape past 0xff does not "
                                       "fit in a char");
        }

        return value;
    }
    if (c >= '0' && c <= '7') {
        value = c - '0';
        for (digits = 1; digits < 3 && *cursor >= '0' && *cursor <= '7'; digits++)
            value = value * 8 + (*cursor++ - '0');
        if (value > 255)
            acc_error_at(tok_line, "an octal escape past \\377 does not fit "
                                   "in a char");

        return value;
    }
    acc_error_at(tok_line, "'\\%c' is not an escape C has", c);
}

/* A character of a literal: an escape, or itself. A newline ends the line
 * the literal was meant to finish on, and the end of the file it all. */
static int literal_char(int quote)
{
    int c = (unsigned char) *cursor;

    if (c == '\0' || c == '\n')
        acc_error_at(tok_line, "a %s is not closed on the line it starts on",
                     quote == '"' ? "string" : "character constant");
    cursor++;

    return c == '\\' ? escape() : c;
}

/* What next() does not recognise as punctuation, the quote or the stray
 * character already consumed: a character constant, which is an int -- and
 * a char's value, so '\377' is -1 where char is signed, as it is here -- a
 * string, or a character that begins nothing. Out of line, where the
 * refusal of a stray character already was, so that punctuation pays
 * nothing for literals it is not. */
__attribute__((noinline))
static void lex_quoted(int c)
{
    int n = 0;

    if (c == '\'') {
        int value;

        if (*cursor == '\'')
            acc_error_at(tok_line, "a character constant needs a character");
        value = literal_char(c);
        if (*cursor != '\'')
            acc_error_at(tok_line, *cursor == '\n' || *cursor == '\0'
                         ? "a character constant is not closed on the line it "
                           "starts on"
                         : "a character constant holds one character; for "
                           "more, use a string");
        cursor++;
        tok = TK_INT;
        tok_type = TY_INT;
        tok_val = (signed char) value;

        return;
    }
    if (c != '"')
        punct_error(c);

    while (*cursor != '"') {
        int ch = literal_char(c);

        if (n + 1 >= str_cap) {
            str_cap = str_cap ? str_cap * 2 : 128;
            str_buf = realloc(str_buf, (size_t) str_cap);
            if (!str_buf)
                acc_error("out of memory for a string");
        }
        str_buf[n++] = (char) ch;
    }
    cursor++;
    tok = TK_STRING;
    tok_str = str_buf;
    tok_str_len = n;
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

    /* A leading `.` can only begin a floating literal. A leading digit might
     * begin either, and which it is shows at the character the digits stop
     * at -- so the integer is read first and handed back if that character
     * says it was one after all. Scanning ahead to decide instead cost every
     * numeric token in the program a second pass over its digits. */
    if (c == '.' && is_digit((unsigned char) cursor[1]))
    {
        lex_floating();

        return;
    }

    if (is_digit(c)) {
        lex_number();

        return;
    }

    if (is_alpha(c)) {
        const char *s = cursor;

        while (is_alnum((unsigned char) *cursor))
            cursor++;
        tok_name = name_intern(s, (int) (cursor - s));
        tok = (tok_name < kw_limit) ? (unsigned char) name_arena[tok_name - 3]
                                    : TK_IDENT;
        return;
    }

    cursor++;
    tok = punct[(unsigned char) c];
    if (tok == TK_EOF) {
        lex_quoted(c);

        return;
    }

    /* Most punctuation can be the first of two or three characters: an
     * operator doubled (`<<`, `&&`, `++`), followed by `=` (`<=`, `+=`), or
     * both (`<<=`). Everything else is one character, so the common case is
     * two compares that both fail rather than a walk through the pairs.
     *
     * Some of what is lexed here is not implemented. It is lexed anyway,
     * because the alternative is not that it is refused later but that it is
     * misread: `a && b` becomes a bitwise and of a with the address of b, and
     * `a ++ b` becomes `a + +b`, and neither says a word about it. */
    if (*cursor == '=' || *cursor == c) {
        int assign = (*cursor == '=');

        switch (c) {
        case '+':
            cursor++;
            tok = assign ? TK_ADD_ASSIGN : TK_INC;
            break;
        case '-':
            cursor++;
            tok = assign ? TK_SUB_ASSIGN : TK_DEC;
            break;
        case '*':
            if (assign) { cursor++; tok = TK_MUL_ASSIGN; }
            break;
        case '/':
            if (assign) { cursor++; tok = TK_DIV_ASSIGN; }
            break;
        case '%':
            if (assign) { cursor++; tok = TK_MOD_ASSIGN; }
            break;
        case '^':
            if (assign) { cursor++; tok = TK_XOR_ASSIGN; }
            break;
        case '&':
            cursor++;
            if (assign) {
                tok = TK_AND_ASSIGN;
                break;
            }
            tok = TK_ANDAND;
            if (*cursor == '=')
                punct_error('&');
            break;
        case '|':
            cursor++;
            if (assign) {
                tok = TK_OR_ASSIGN;
                break;
            }
            tok = TK_OROR;
            if (*cursor == '=')
                punct_error('|');
            break;
        case '<':
            cursor++;
            if (assign) {
                tok = TK_LE;
                break;
            }
            tok = TK_SHL;
            if (*cursor == '=') { cursor++; tok = TK_SHL_ASSIGN; }
            break;
        case '>':
            cursor++;
            if (assign) {
                tok = TK_GE;
                break;
            }
            tok = TK_SHR;
            if (*cursor == '=') { cursor++; tok = TK_SHR_ASSIGN; }
            break;
        case '=':
            if (assign) { cursor++; tok = TK_EQ; }
            break;
        case '!':
            if (assign) { cursor++; tok = TK_NE; }
            break;
        }
    }
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
    case TK_KW_CHAR:   return "'char'";
    case TK_KW_SHORT:  return "'short'";
    case TK_KW_LONG:   return "'long'";
    case TK_KW_SIGNED: return "'signed'";
    case TK_KW_UNSIGNED: return "'unsigned'";
    case TK_KW_FLOAT:  return "'float'";
    case TK_KW_DOUBLE: return "'double'";
    case TK_KW_FOR:    return "'for'";
    case TK_KW_GOTO:   return "'goto'";
    case TK_KW_SIZEOF: return "'sizeof'";
    case TK_STRING:    return "a string";
    case TK_KW_BREAK:  return "'break'";
    case TK_KW_CONTINUE: return "'continue'";
    case TK_KW_DO:     return "'do'";
    case TK_KW_SWITCH: return "'switch'";
    case TK_KW_CASE:   return "'case'";
    case TK_KW_DEFAULT: return "'default'";
    case TK_KW_RESERVED: return "a reserved word";
    case TK_FLOAT:     return "a floating-point number";
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
    case TK_ANDAND:    return "'&&'";
    case TK_OROR:      return "'||'";
    case TK_ADD_ASSIGN: return "'+='";
    case TK_SUB_ASSIGN: return "'-='";
    case TK_MUL_ASSIGN: return "'*='";
    case TK_DIV_ASSIGN: return "'/='";
    case TK_MOD_ASSIGN: return "'%='";
    case TK_AND_ASSIGN: return "'&='";
    case TK_OR_ASSIGN:  return "'|='";
    case TK_XOR_ASSIGN: return "'^='";
    case TK_SHL_ASSIGN: return "'<<='";
    case TK_SHR_ASSIGN: return "'>>='";
    case TK_QUESTION:  return "'?'";
    case TK_LBRACKET:  return "'['";
    case TK_RBRACKET:  return "']'";
    case TK_COLON:     return "':'";
    case TK_INC:       return "'++'";
    case TK_DEC:       return "'--'";
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

int accept_next(void)
{
    next();

    return 1;
}

/* Point at where the missing token should have gone, which is the end of the
 * token before it, not the start of whatever turned up instead: a ';' left
 * off the end of line 4 is a mistake on line 4, even though the '}' that
 * reveals it is on line 5. */
void expect_failed(const char *what)
{
    acc_error_at(tok_prev_line, "expected %s, found %s", what, tok_spelling(tok));
}

void lex_init(void)
{
    keywords_init();
}
