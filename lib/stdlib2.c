/*
 * The rest of <stdlib.h>: rand and srand, bsearch, getenv and system.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int acc_rt_mos(int fn, int hl, int de, int bc);

/* agondev's generator, so that a seed gives a program built by acc the
 * numbers it gives one built by agondev: sixteen bytes of state, a xorshift
 * of its first word into its last, answering the sum of its last two
 * words; rand the low 23 bits of that, less the carry out of the sum, as
 * libagon's random.s leaves it. Seeded with the bytes 16 down to 1 until
 * srand puts its seed in the first three and 12 down to 1 after a zero. */
static unsigned char state[16] = {
    16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1
};

static unsigned long word_at(int i)
{
    return state[i] | (unsigned long) state[i + 1] << 8
           | (unsigned long) state[i + 2] << 16 | (unsigned long) state[i + 3] << 24;
}

int rand(void)
{
    unsigned char x0 = state[0], x1 = state[1], x2 = state[2], x3 = state[3];
    unsigned char s12 = state[12], s13 = state[13], s14 = state[14];
    unsigned t = (unsigned) ((x1 << 8 | x0) << 3) & 0xFFFF;
    unsigned char h = (unsigned char) (x3 ^ (t >> 8)), l = (unsigned char) (x2 ^ t);
    unsigned mixed = ((unsigned) h << 8 | l) << 3;
    unsigned long sum, high, value;
    unsigned carry;

    memmove(state, state + 4, 12);
    state[15] = (unsigned char) (h ^ s14);
    state[14] = (unsigned char) (l ^ s13);
    state[13] = (unsigned char) ((mixed >> 16) ^ x1 ^ s12);
    state[12] = (unsigned char) (x0 ^ (mixed >> 8));
    high = word_at(12);
    sum = high + word_at(8);
    carry = (sum & 0xFFFFFFFFUL) < high;
    value = (sum & 0xFFFFFFUL) - 0x800000UL - carry;
    if (value & 0x800000UL)
        value += 0x800000UL;

    return (int) (value & 0x7FFFFFUL);
}

void srand(unsigned seed)
{
    int i;

    state[0] = (unsigned char) seed;
    state[1] = (unsigned char) (seed >> 8);
    state[2] = (unsigned char) (seed >> 16);
    state[3] = 0;
    for (i = 4; i != 16; i++)
        state[i] = (unsigned char) (16 - i);
}

/* The element of the sorted array equal to `key`, by halving. */
void *bsearch(const void *key, const void *base, size_t n, size_t size,
              int (*cmp)(const void *, const void *))
{
    const char *lo = base;

    while (n) {
        const char *mid = lo + (n / 2) * size;
        int c = cmp(key, mid);

        if (!c)
            return (void *) mid;
        if (c > 0) {
            lo = mid + size;
            n -= n / 2 + 1;
        } else {
            n /= 2;
        }
    }

    return NULL;
}

/* MOS has no environment, so there is no name in it. */
char *getenv(const char *name)
{
    (void) name;

    return NULL;
}

/* A command for MOS's own command line, which is the command processor
 * here: system(NULL) says there is one, and anything else is what MOS
 * answers -- 0 for done, or its error number. A command that runs a
 * program loads it over this one, unless it is a moslet. */
int system(const char *command)
{
    if (!command)
        return 1;

    return acc_rt_mos(0x10, (int) command, 0, 0) & 0xff;       /* mos_oscli */
}
