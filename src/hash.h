/*
 * The hash of a name, which places it in the name table (names.c).
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The host build hashes with this. The Agon build hashes with src/hash.s,
 * the same hash written by hand, and test/hash.sh builds a program with
 * both and holds the one to the other on the emulator.
 */
#ifndef ACC_HASH_H
#define ACC_HASH_H

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

/* A name's two hash bytes: high, mixed, in the low byte of the answer, and
 * low in the byte above it -- the two that name_home masks into a slot's
 * offset. */
static inline unsigned name_hash_c(const char *text, unsigned n)
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
    const unsigned char *at = (const unsigned char *) text, *end = at + n;

    /* A pointer walked to the end rather than an index: the index was an add
     * to the base on every character, and the base and the count were read
     * back from the frame on every one -- a fifth of a compile went through
     * this loop. On the Agon it is src/hash.s, in registers. */
    while (at != end) {
        unsigned char c = *at++;

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

    return (unsigned) low << 8 | high;
}

#endif
