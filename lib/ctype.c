/*
 * ctype: what a byte is, in the only locale acc has.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * C99 7.4, in the "C" locale, which is the one every program here is in and
 * the only one the library has: the letters are the twenty-six either way,
 * and nothing above 127 is any of these things. The Agon's screen font has
 * characters up there and MOS has its own ideas about them; a program that
 * wants to know about those wants a table of its own, not this.
 *
 * Written as comparisons rather than as a table of 257 flags. The table is
 * the faster answer on a machine with an index register and a cheap add,
 * and this one has neither: a lookup is a 16-bit add and a load, where
 * `c - '0' <= 9` on an unsigned byte is two instructions. It would also be
 * 257 bytes in every program that asked any one of these.
 *
 * Each takes an int because C99 says so -- EOF has to be tellable from every
 * byte -- and each is written so that a negative one falls through every
 * test rather than reaching past anything.
 */
#include <ctype.h>

/* The unsigned window: c is one of the 256 bytes, or it is none of them.
 * Written once so that the tests below read as the ranges they are. */
#define in(c, lo, hi)   ((unsigned int) ((c) - (lo)) <= (unsigned int) ((hi) - (lo)))

int isalnum(int c)
{
    return isalpha(c) || isdigit(c);
}

int isalpha(int c)
{
    return islower(c) || isupper(c);
}

int isblank(int c)
{
    return c == ' ' || c == '\t';
}

int iscntrl(int c)
{
    return in(c, 0, 0x1f) || c == 0x7f;
}

int isdigit(int c)
{
    return in(c, '0', '9');
}

int isgraph(int c)
{
    return in(c, '!', '~');
}

int islower(int c)
{
    return in(c, 'a', 'z');
}

int isprint(int c)
{
    return in(c, ' ', '~');
}

int ispunct(int c)
{
    return isgraph(c) && !isalnum(c);
}

int isspace(int c)
{
    return c == ' ' || in(c, '\t', '\r');   /* tab, nl, vt, ff, cr */
}

int isupper(int c)
{
    return in(c, 'A', 'Z');
}

int isxdigit(int c)
{
    return isdigit(c) || in(c, 'a', 'f') || in(c, 'A', 'F');
}

int tolower(int c)
{
    return isupper(c) ? c + ('a' - 'A') : c;
}

int toupper(int c)
{
    return islower(c) ? c - ('a' - 'A') : c;
}
