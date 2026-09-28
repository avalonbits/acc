/*
 * The character-class table, over every byte.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 *
 * The table replaced four range tests per character with one lookup, and a
 * lookup is only as right as its 256 entries. The programs the other tests
 * compile use perhaps forty distinct characters between them, so they would
 * not notice a wrong entry for '`' or for 0x80 -- the first would make a stray
 * character start an identifier, the second would depend on whether char is
 * signed. Here the table is checked against the definitions it stands for.
 */

#include <stdio.h>

#define ACC_CTYPE_TABLE                 /* the table itself, as lex.c has it */
#include "ctype.h"

static int failures = 0;

/* What the code said before the table did -- and, since universal
 * character names, that every byte from 0x80 up is a letter too: UTF-8's,
 * which is how such a name reaches the lexer. */
static int ref_space(int c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static int ref_digit(int c) { return c >= '0' && c <= '9'; }
static int ref_alpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80; }
static int ref_alnum(int c) { return ref_alpha(c) || ref_digit(c); }

static void check(const char *what, int c, int got, int want)
{
    if (!got != !want) {
        fprintf(stderr, "  FAIL %s(%d) gave %d, want %d\n", what, c, !!got, !!want);
        failures++;
    }
}

int main(void)
{
    int c;

    for (c = 0; c < 256; c++) {
        check("is_space", c, is_space(c), ref_space(c));
        check("is_digit", c, is_digit(c), ref_digit(c));
        check("is_alpha", c, is_alpha(c), ref_alpha(c));
        check("is_alnum", c, is_alnum(c), ref_alnum(c));
    }

    /* char is signed on the eZ80 and on the host build, so the lexer hands
     * these in as `(unsigned char) *p`. If the cast inside the macro were
     * dropped, a byte above 127 would index before the table rather than
     * finding the letter it is. */
    for (c = 128; c < 256; c++) {
        char signed_c = (char) c;

        check("is_alnum, high byte", c, is_alnum(signed_c), 1);
        check("is_space, high byte", c, is_space(signed_c), 0);
    }

    if (failures)
        fprintf(stderr, "  %d failed\n", failures);
    else
        fprintf(stderr, "  1024 classifications, 0 failed\n");

    return failures ? 1 : 0;
}
