/*
 * Tokens: the current one and what it holds, skipping blanks and comments,
 * keywords, numbers, character and string literals, punctuators, and
 * next().
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#define ACC_CTYPE_TABLE                 /* the one copy: see ctype.h */
#include "ctype.h"
#include "lex_int.h"

int      tok;
long     tok_val;
uint32_t tok_val_hi;
float    tok_fval;
typedef char float_is_four_bytes[sizeof(float) == 4 ? 1 : -1];
NameRef  tok_name;
int      tok_line;
const char *tok_at;
Type     tok_type;
int      tok_prev_line;

/* A comment, with the cursor on its opening `/`. Out of line so that
 * skip_space, which is inlined into next(), carries nothing but the cursor
 * and the line count: the comment scan's own state, the line a block comment
 * opened on, was enough to give next() a stack frame -- on every token,
 * where comments are one call each. */
__attribute__((noinline))
void skip_comment(void)
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
    const char *open_at = cursor;       /* gone once the window moves */

    cursor += 2;
    for (;;) {
        while (*cursor && !(cursor[0] == '*' && cursor[1] == '/')) {
            if (*cursor == '\n')
                line++;
            cursor++;
        }
        if (*cursor)
            break;

        /* The window ended inside the comment. The two characters that close
         * one cannot have a newline between them and a window always ends
         * after a newline, so a `*<slash>` is never split by this. */
        if (!refill())
            acc_error_pos(opened, column_of(open_at), "unterminated comment");
        open_at = NULL;
    }
    cursor += 2;
}

/* Declared here as well as where it is set: the two names the compiler
 * defines are keywords, and what asks whether a name is one of them comes
 * before the keywords do. A second tentative definition of the same object
 * is what C has for exactly this. */
NameRef kw_limit;

__attribute__((noinline))
void skip_space(void)
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
NameRef kw_limit;

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
    keyword("auto", 4, TK_KW_AUTO);
    keyword("break", 5, TK_KW_BREAK);
    keyword("case", 4, TK_KW_CASE);
    keyword("const", 5, TK_KW_CONST);
    keyword("continue", 8, TK_KW_CONTINUE);
    keyword("default", 7, TK_KW_DEFAULT);
    keyword("do", 2, TK_KW_DO);
    keyword("enum", 4, TK_KW_ENUM);
    keyword("extern", 6, TK_KW_EXTERN);
    keyword("goto", 4, TK_KW_GOTO);
    keyword("inline", 6, TK_KW_INLINE);
    keyword("register", 8, TK_KW_REGISTER);
    keyword("restrict", 8, TK_KW_RESTRICT);
    keyword("sizeof", 6, TK_KW_SIZEOF);
    keyword("static", 6, TK_KW_STATIC);
    keyword("struct", 6, TK_KW_STRUCT);
    keyword("switch", 6, TK_KW_SWITCH);
    keyword("typedef", 7, TK_KW_TYPEDEF);

    /* Not words of the language but names the preprocessor defines, which
     * is why they are here rather than among the macros: a macro costs a
     * lookup for every identifier in the program, and a keyword costs the
     * one the lexer was doing anyway. C forbids a program from defining or
     * undefining either, so nothing is lost by their not being in the
     * table. */
    keyword("__FILE__", 8, TK_FILE);
    keyword("__LINE__", 8, TK_LINE);
    keyword("__DATE__", 8, TK_DATE);
    keyword("__TIME__", 8, TK_TIME);
    keyword("__STDC__", 8, TK_STDC);
    keyword("__STDC_VERSION__", 16, TK_STDC_VERSION);
    keyword("__STDC_HOSTED__", 15, TK_STDC_HOSTED);
    keyword("_Pragma", 7, TK_PRAGMA_OP);
    keyword("union", 5, TK_KW_UNION);
    keyword("volatile", 8, TK_KW_VOLATILE);
    keyword("_Bool", 5, TK_KW_BOOL);
    keyword("_Static_assert", 14, TK_KW_STATIC_ASSERT);
    keyword("__builtin_offsetof", 18, TK_KW_OFFSETOF);
    keyword("__attribute__", 13, TK_KW_ATTRIBUTE);

    /* gcc's spellings of restrict, which headers written for it use where
     * restrict cannot go -- before C99, or in C++ -- and which are the
     * implementation's to give a meaning to. Read as a name, one was a
     * parameter's: `void *memcpy(void *__restrict, const void
     * *__restrict, size_t)` named two parameters the same. */
    keyword("__restrict", 10, TK_KW_RESTRICT);
    keyword("__restrict__", 12, TK_KW_RESTRICT);

    /* What <stdarg.h> is made of, as words of the language: va_arg has to
     * know the width of what it reads to step over it, which no macro
     * written in C can tell it. Spelled as names reserved to the
     * implementation, so that va_list and the rest are <stdarg.h>'s to
     * define -- a type and four macros, as C99 7.15 has them -- and a
     * program that does not include it may use the names for itself. */
    keyword("__va_list", 9, TK_KW_VA_LIST);
    keyword("__va_start", 10, TK_KW_VA_START);
    keyword("__va_arg", 8, TK_KW_VA_ARG);
    keyword("__va_end", 8, TK_KW_VA_END);
    keyword("__va_copy", 9, TK_KW_VA_COPY);
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
    ['*'] = TK_STAR,   ['/'] = TK_SLASH,
    ['&'] = TK_AMP,    ['|'] = TK_PIPE,   ['^'] = TK_CARET,
    ['~'] = TK_TILDE,
    ['>'] = TK_GT,
    ['?'] = TK_QUESTION,
    ['['] = TK_LBRACKET, [']'] = TK_RBRACKET,
    ['.'] = TK_DOT
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

    /* An l suffix would make it a long double, which acc does not have: said
     * here rather than left to the parser, which would see the letter as a
     * name of its own and blame the punctuation. */
    if (*end == 'l' || *end == 'L')
        acc_error_at(tok_line, "'long double' is not supported: agondev's "
                               "library has no arithmetic for one, so a "
                               "program that asks for it does not link");

    memcpy(&tok_fval, &bits, sizeof tok_fval);
    cursor = (char *) end;
    if (*cursor == 'f' || *cursor == 'F')
        cursor++;
    tok = TK_FLOAT;
    tok_type = TY_FLOAT;
}

/* A constant past 32 bits, read again from its start as the long long it
 * is: its high half returned and its low half in *low. Out of line, and
 * rare, because its 64-bit steps are calls into the runtime on the Agon. */
__attribute__((noinline))
static uint32_t wide_constant(const char *p, uint32_t *low)
{
    uint64_t value = 0, limit;
    unsigned base = 10;

    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        base = 16;
        p += 2;
    } else if (p[0] == '0') {
        base = 8;
    }
    limit = UINT64_MAX / base;
    for (;; p++) {
        int c = (unsigned char) *p, digit;

        if (is_digit(c))
            digit = c - '0';
        else if (base == 16 && (c | 0x20) >= 'a' && (c | 0x20) <= 'f')
            digit = (c | 0x20) - 'a' + 10;
        else
            break;
        if (value > limit || value * base > UINT64_MAX - (unsigned) digit)
            acc_error_at(tok_line, "the constant does not fit in 64 bits");
        value = value * base + (unsigned) digit;
    }
    *low = (uint32_t) value;

    return (uint32_t) (value >> 32);
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
    uint32_t value = 0, high = 0;
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

            /* A suffix, which the loop used to read as a digit and refuse:
             * 0xffu and 0x10L were errors. No hex digit is u or l. Nor is
             * p, which is a hex float's exponent: C99 lets one go without
             * a point, as 0x1p-126 does, and the loop refused that too. */
            if ((digit | 0x20) == 'u' || (digit | 0x20) == 'l'
                || (digit | 0x20) == 'p')
                break;
            if (is_digit(digit))                     digit -= '0';
            else if (digit >= 'a' && digit <= 'f')   digit -= 'a' - 10;
            else if (digit >= 'A' && digit <= 'F')   digit -= 'A' - 10;
            else acc_error_at(tok_line, "bad digit '%c' in a hex constant", digit);
            if (value > 0xfffffffUL)
                overflowed = 1;         /* a long long: wide_constant */
            else
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
    /* The character the digits stopped at decides which kind it was. A
     * hex constant's digits take in e and f, so only a hex one stops at p. */
    if (*cursor == '.' || *cursor == 'e' || *cursor == 'E'
        || *cursor == 'f' || *cursor == 'F'
        || ((*cursor | 0x20) == 'p' && (start[1] | 0x20) == 'x')) {
        cursor = start;
        lex_floating();

        return;
    }

    if (bad_digit)
        acc_error_at(tok_line, "'%c' is not an octal digit, and a constant that "
                           "starts with 0 is octal", bad_digit);
    if (overflowed)
        high = wide_constant(start, &value);

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
        suffix_l = 1;
        if (cursor[1] == *cursor) {     /* ll or LL, but not lL */
            suffix_l = 2;
            cursor++;
        }
        cursor++;
    }
    if (is_alnum((unsigned char) *cursor))
        acc_error_at(tok_line, "'%c' is not a suffix a constant can have", *cursor);

    /* C99 types a constant by the first type in that list that can hold it.
     * The unsigned types are in it when the suffix says u, and also when a
     * hex or octal constant has no suffix at all -- which is the only place
     * the decimal and hex forms differ. Past the end of long it is long
     * long, below. */
    if (high || suffix_l == 2) {
        /* long long, which only the steps past long reach */
    } else if (suffix_u) {
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

    /* The long long steps: signed if it fits and nothing says unsigned.
     * A decimal constant past the signed range has no type in C99; it is
     * taken as unsigned, which is what agondev does with it. */
    if (tok_type == TY_VOID)
        tok_type = (!suffix_u && high <= 0x7fffffffUL) ? TY_LLONG : TY_ULLONG;

    /* An unsigned int is normalised to the signed pattern of the same 24
     * bits, so that acc folds it identically whether it is itself running on
     * a 24-bit int or a 32-bit one. */
    if (tok_type == TY_UINT && value > 0x7fffffUL)
        value -= 0x1000000UL;

    tok = TK_INT;
    tok_val = (long) value;
    tok_val_hi = high;
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
int         tok_str_wide;
int         tok_str_escaped;

static char *str_buf;
static int   str_cap;

#define is_hexdigit(c) (is_digit(c) || ((unsigned) (((c) | 0x20) - 'a') < 6u))

/* A universal character name that reached a token still written as one,
 * at p: every one that names a character C allows was made UTF-8 when its
 * window was read, so this one either is short of digits or names one of
 * the characters 6.4.3p2 forbids. */
__attribute__((noinline, noreturn))
static void ucn_refuse(const char *p)
{
    int n = ucn_at(p);

    if (!n)
        acc_error_at(tok_line, "'\\%c' is a universal character name, and "
                               "needs %d hex digits after it", p[1],
                     p[1] == 'u' ? 4 : 8);
    acc_error_at(tok_line, "\\%c%.*s is not a character a universal "
                           "character name may name", p[1], n - 2, p + 2);
}

/* One character of a literal, the backslash of an escape read already: the
 * byte it stands for. Octal takes up to three digits and hex as many as
 * there are, which is what C says; a value past a byte is refused. */
static int escape(int wide)
{
    int c = (unsigned char) *cursor++, value, digits;
    int most = wide ? 0xffff : 0xff;

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
            if (value > most)
                acc_error_at(tok_line, wide ? "a '\\x' escape past 0xffff "
                                              "does not fit in a wchar_t"
                                            : "a '\\x' escape past 0xff does "
                                              "not fit in a char");
        }
        if (value > 0x7f)
            tok_str_escaped = 1;        /* see string_gather */

        return value;
    }
    if (c >= '0' && c <= '7') {
        value = c - '0';
        for (digits = 1; digits < 3 && *cursor >= '0' && *cursor <= '7'; digits++)
            value = value * 8 + (*cursor++ - '0');
        if (value > most)
            acc_error_at(tok_line, "an octal escape past \\377 does not fit "
                                   "in a char");
        if (value > 0x7f)
            tok_str_escaped = 1;        /* see string_gather */

        return value;
    }
    if (c == 'u' || c == 'U')
        ucn_refuse(cursor - 2);
    acc_error_at(tok_line, "'\\%c' is not an escape C has", c);
}

/* A character of a literal: an escape, or itself. A newline ends the line
 * the literal was meant to finish on, and the end of the file it all. */
static int literal_char(int quote)
{
    int c;

    c = (unsigned char) *cursor;

    if (c == '\0' || c == '\n')
        acc_error_at(tok_line, "a %s is not closed on the line it starts on",
                     quote == '"' ? "string" : "character constant");
    cursor++;

    return c == '\\' ? escape(0) : c;
}

/* What next() does not recognise as punctuation, the quote or the stray
 * character already consumed: a character constant, which is an int -- and
 * a char's value, so '\377' is -1 where char is signed, as it is here -- a
 * string, or a character that begins nothing. Out of line, where the
 * refusal of a stray character already was, so that punctuation pays
 * nothing for literals it is not. */
static void lex_two(int c);
static void lex_digraph(int c);

__attribute__((noinline))
static void lex_quoted(int c)
{
    int n = 0;

    /* '<', '%' and ':', which begin the digraphs as well as their own
     * tokens and pairs. They come here, out of line, rather than through
     * the table next() reads the others from. Testing for the digraphs in
     * next() cost 0.7% of every compile -- not in the test, but in what it
     * did to the registers of the loop that reads a name. Here they cost a
     * call each, 0.26%, and next() compiles as it did. */
    if (c == '\\' && (*cursor == 'u' || *cursor == 'U'))
        ucn_refuse(cursor - 1);
    if (c == '<' || c == '%' || c == ':') {
        tok = c == '<' ? TK_LT : c == '%' ? TK_PERCENT : TK_COLON;
        if (*cursor == '=' || *cursor == c)
            lex_two(c);
        else if (*cursor == ':' || *cursor == '%' || *cursor == '>')
            lex_digraph(c);

        return;
    }

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

    tok_str_escaped = 0;                /* escape() sets it */
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
    tok_str_wide = 0;
}

/* _Pragma ( string-literal ), C99 6.10.9: the string with its quotes and
 * escapes taken off, done as a #pragma would do it -- which here means
 * `once` is acted on and anything else is ignored, as C says an unknown
 * pragma is. It is a unary operator that leaves nothing behind, so the
 * token after it is read to take its place. */
void pragma_operator(void)
{
    int once;

    next();
    if (tok != TK_LPAREN)
        acc_error_at(tok_line, "_Pragma needs a string in parentheses");
    next();
    if (tok != TK_STRING || tok_str_wide)
        acc_error_at(tok_line, "_Pragma needs a string in parentheses");
    once = tok_str_len >= 4 && !strncmp(tok_str, "once", 4);
    while (once && tok_str_len > 4 && is_space(tok_str[tok_str_len - 1]))
        tok_str_len--;
    if (once && tok_str_len == 4)
        once_add(src_real);
    next();
    if (tok != TK_RPAREN)
        acc_error_at(tok_line, "_Pragma needs a string in parentheses");
    next();
}

/* One character of a wide literal: an escape, which may be as wide as a
 * wchar_t, or a character of the source -- UTF-8, which a universal
 * character name has been made into already, taken apart into the
 * character it spells. */
static uint32_t wide_char(int quote)
{
    int c = (unsigned char) *cursor, n, i;
    uint32_t v;

    if (c == '\0' || c == '\n')
        acc_error_at(tok_line, "a %s is not closed on the line it starts on",
                     quote == '"' ? "string" : "character constant");
    cursor++;
    if (c == '\\')
        return (uint32_t) escape(1);
    if (c < 0x80)
        return (uint32_t) c;

    n = c >= 0xf0 && c < 0xf8 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : -1;
    if (n < 0)
        acc_error_at(tok_line, "a wide literal is read as UTF-8, and this "
                               "byte cannot begin a character in it");
    v = (uint32_t) (c & (0x3f >> n));
    for (i = 0; i != n; i++) {
        int d = (unsigned char) *cursor;

        if ((d & 0xc0) != 0x80)
            acc_error_at(tok_line, "a wide literal is read as UTF-8, and a "
                                   "character in it stops short");
        cursor++;
        v = v << 6 | (uint32_t) (d & 0x3f);
    }

    return v;
}

static void wide_unit(int *n, uint32_t v)
{
    if (*n + 2 >= str_cap) {
        str_cap = str_cap ? str_cap * 2 : 128;
        str_buf = realloc(str_buf, (size_t) str_cap);
        if (!str_buf)
            acc_error("out of memory for a string");
    }
    str_buf[(*n)++] = (char) (v & 0xff);
    str_buf[(*n)++] = (char) (v >> 8);
}

/* A wide character constant or a wide string, the L read and the cursor on
 * the quote after it (C99 6.4.4.4, 6.4.5). A wchar_t is agondev's, a short,
 * so a wide string is UTF-16 -- a character past 0xffff is the two halves
 * of a surrogate pair, as agondev writes it -- and a wide character
 * constant past it is refused, as agondev refuses it. The string's units go
 * into the same buffer as a narrow string's bytes, two bytes each, low
 * first. */
void wide_literal(void)
{
    int quote = *cursor, n = 0;
    uint32_t v;

    cursor++;

    if (quote == '\'') {
        if (*cursor == '\'')
            acc_error_at(tok_line, "a character constant needs a character");
        v = wide_char(quote);
        if (v > 0xffff)
            acc_error_at(tok_line, "a wide character constant past 0xffff "
                                   "does not fit in a wchar_t");
        if (*cursor != '\'')
            acc_error_at(tok_line, *cursor == '\n' || *cursor == '\0'
                         ? "a character constant is not closed on the line it "
                           "starts on"
                         : "a character constant holds one character; for "
                           "more, use a string");
        cursor++;
        tok = TK_INT;
        tok_type = TY_SHORT;
        tok_val = (short) v;
        tok_val_hi = 0;

        return;
    }

    while (*cursor != '"') {
        v = wide_char(quote);
        if (v > 0xffff) {
            wide_unit(&n, 0xd800 + ((v - 0x10000) >> 10));
            wide_unit(&n, 0xdc00 + ((v - 0x10000) & 0x3ff));
        } else {
            wide_unit(&n, v);
        }
    }
    cursor++;
    tok = TK_STRING;
    tok_str = str_buf;
    tok_str_len = n;
    tok_str_wide = 1;
}

/* The second character of a two- or three-character operator, the first
 * having been read as `c` and its one-character token set. Out of line, so
 * that next() -- which runs for every token -- needs no frame: its locals
 * were only for this, and the frame was a call on every token. */
__attribute__((noinline))
static void lex_two(int c)
{
    int assign = (*cursor == '=');

    if (c == '.') {                     /* `..`, which only `...` may be */
        if (cursor[1] != '.')
            acc_error_at(tok_line, "'..' is not something C has; '...' is");
        cursor += 2;
        tok = TK_ELLIPSIS;

        return;
    }
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

/* The digraphs, C99 6.4.6: `<:` `:>` `<%` `%>` are the brackets and braces,
 * and `%:` is '#'. The cursor is past c, on the character that may make it
 * one. A %: first on its line is a directive, read here, and the token after
 * it is read with a call back into next(), which is where this came from. */
static void lex_digraph(int c)
{
    int second = *cursor;

    if (c == '<' && second == ':') { cursor++; tok = TK_LBRACKET; return; }
    if (c == '<' && second == '%') { cursor++; tok = TK_LBRACE;   return; }
    if (c == ':' && second == '>') { cursor++; tok = TK_RBRACKET; return; }
    if (c == '%' && second == '>') { cursor++; tok = TK_RBRACE;   return; }
    if (c == '%' && second == ':') {
        if (src_macro == NAME_NONE && cursor[1] != '%'
            && at_line_start(cursor - 1)) {
            cursor--;
            directives();
            next();

            return;
        }
        acc_error_at(tok_line, "stray '%%:' in the source: it is '#', which "
                               "is only a directive first on its line");
    }
}                                       /* `<>` `:%` and the like: two tokens */

/* The parentheses after `__attribute__`, which the standard doubles: read
 * tokens and count depth until the pair that opened it closes. Recursing
 * into next() is what lets the contents be anything at all, including a
 * macro that expands to more of them. */
__attribute__((noinline))
static void skip_attribute(void)
{
    int depth = 1;

    next();
    if (tok != TK_LPAREN)
        acc_error_at(tok_line, "expected '(' after '__attribute__'");

    while (depth > 0) {
        next();
        if (tok == TK_LPAREN)
            depth++;
        else if (tok == TK_RPAREN)
            depth--;
        else if (tok == TK_EOF)
            acc_error_at(tok_line, "unterminated '__attribute__'");
    }
}

/* What next() does with a character that is no punctuator: a directive, if
 * it is a `#` first on its line, and otherwise the quoted literal it must
 * begin. Out of line: inlined, the walk back over the line in at_line_start
 * wanted a frame slot of its own, which next() then built on every token.
 * Returns whether a directive was read, and the next token is still to
 * find. */
__attribute__((noinline))
static int not_punct(int c)
{
    if (c == '#' && src_macro == NAME_NONE && at_line_start(cursor - 1)) {
        cursor--;
        directives();

        return 1;
    }
    lex_quoted(c);

    return 0;
}

void next(void)
{
    int c;

    /* Before the label: a directive, an attribute or a macro read on the way
     * to the next token is not the token before it. */
    tok_prev_line = tok_line;

    /* A macro comes back here rather than calling next() again. The two say
     * the same thing -- the call would re-run everything below the label --
     * but a function that calls itself is one clang gives a frame and spills
     * around, and this one is where a compile spends its time. */
restart:
    skip_space();

    /* The end of the window, which is the one place the buffer moves. The
     * test is here rather than in skip_space because every token goes
     * through that loop, and the reading itself is out of line because it
     * happens once per 16 KB. */
    if (!*cursor)
        window_more();

    tok_line = line;
    tok_at = cursor;
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

        if (tok_name < kw_limit) {
            tok = (unsigned char) name_arena[tok_name - 3];

            /* The names the compiler defines, whose value is not the word:
             * they are the last codes in the enum, so this is one compare
             * on the path every keyword in the program takes. */
            if (tok >= TK_FILE)
                predefined();

            /* An attribute says nothing acc acts on -- `noinline` and
             * `always_inline` are advice to an optimiser that is not here,
             * and `noreturn` only lets one be quieter about a return that
             * never happens. It is thrown away here rather than in the
             * parser because it is allowed in more places than a parser this
             * shape has hooks for: before a declaration, after it, between
             * the specifiers, on a struct member, on a parameter. Skipping
             * whole tokens also means the text inside it -- strings, commas,
             * numbers -- needs no rules of its own. */
            if (tok == TK_KW_ATTRIBUTE) {
                skip_attribute();

                goto restart;
            }

            return;
        }

        /* A name that stands for something else. The byte in front of it
         * says whether it is a macro, which for most names is the only
         * question the preprocessor ever costs them. Being a keyword is
         * already ruled out above, which is one compare this used to make
         * and no longer does. */
        tok = TK_IDENT;
        if (name_is_macro(tok_name) && expand(tok_name))
            goto restart;

        return;
    }

    cursor++;
    tok = punct[(unsigned char) c];
    if (tok == TK_EOF) {
        /* A '#' first on its line is a directive. Asking here rather than
         * before every token costs nothing at all: a name or a number has
         * already returned, and '#' is not a punctuator acc has, so an
         * unknown character is where it was going anyway.
         *
         * Whether it is first on its line is read back off the buffer --
         * walk behind it over blanks and see whether a line begins there --
         * rather than kept in a flag, which would be a store per token. The
         * characters it walks over are always there: a window only ever
         * begins where a line does, so the start of this line is either in
         * it or is the window's own start.
         *
         * Not inside a macro, whose text has no lines of its own and whose
         * '#' will mean something else once there is a '#' to mean. */
        if (not_punct(c))
            goto restart;

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
        lex_two(c);
    } else if (c == '-' && *cursor == '>') {
        cursor++;
        tok = TK_ARROW;
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
    case TK_KW_ENUM:   return "'enum'";
    case TK_KW_STRUCT: return "'struct'";
    case TK_KW_UNION:  return "'union'";
    case TK_KW_TYPEDEF: return "'typedef'";
    case TK_KW_STATIC: return "'static'";
    case TK_KW_EXTERN: return "'extern'";
    case TK_KW_CONST:  return "'const'";
    case TK_KW_AUTO:   return "'auto'";
    case TK_KW_REGISTER: return "'register'";
    case TK_KW_VOLATILE: return "'volatile'";
    case TK_KW_RESTRICT: return "'restrict'";
    case TK_KW_INLINE: return "'inline'";
    case TK_KW_BOOL:   return "'_Bool'";
    case TK_ELLIPSIS:  return "'...'";
    case TK_KW_VA_LIST: return "'va_list'";      /* as <stdarg.h> spells them */
    case TK_KW_VA_START: return "'va_start'";
    case TK_KW_VA_ARG: return "'va_arg'";
    case TK_KW_VA_END: return "'va_end'";
    case TK_KW_VA_COPY: return "'va_copy'";
    case TK_DOT:       return "'.'";
    case TK_ARROW:     return "'->'";
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
    acc_error_pos(tok_prev_line, lex_prev_col(), "expected %s, found %s", what,
                  tok_spelling(tok));
}

void lex_init(void)
{
    keywords_init();
    name_is_macro(name_intern("L", 1)) |= NAME_WIDE;
    predefined_macros_init();
}
