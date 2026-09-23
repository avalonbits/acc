/*
 * What kind of character this is.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * In a header so the table can be checked against the definitions it stands
 * for, over all 256 bytes, rather than against whichever characters the test
 * programs happen to contain.
 */

#ifndef ACC_CTYPE_H
#define ACC_CTYPE_H

/* One table lookup per character instead of up to eight comparisons.
 *
 * is_alnum was `is_alpha(c) || is_digit(c)`, four range tests, and it runs for
 * every character of every identifier in the program. The eZ80 has no
 * instruction for any of that beyond the compares themselves, so the way to
 * make it cheaper is to have done it already: an indexed load from 256 bytes
 * is one instruction.
 *
 * The AND that follows stays 8-bit -- the value comes out of memory in A and
 * the mask is a constant -- so it is a real `and` and not the `call __iand`
 * that masking a 24-bit value costs.
 *
 * Every byte from 0x80 up is a letter. C99 6.4.2.1 lets an implementation
 * take other characters into names, and those bytes are what UTF-8 spells
 * them with -- which is what a universal character name is turned into
 * before anything scans it, so that \u00e9 in a name and an e-acute
 * typed as its UTF-8 are the same name.
 */
#define CH_ALPHA 1
#define CH_DIGIT 2
#define CH_SPACE 4

static const unsigned char ctype[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0, 4, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

#define is_digit(c) (ctype[(unsigned char) (c)] & CH_DIGIT)
#define is_alpha(c) (ctype[(unsigned char) (c)] & CH_ALPHA)
#define is_alnum(c) (ctype[(unsigned char) (c)] & (CH_ALPHA | CH_DIGIT))
#define is_space(c) (ctype[(unsigned char) (c)] & CH_SPACE)

#endif /* ACC_CTYPE_H */
