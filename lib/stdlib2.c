/*
 * The rest of <stdlib.h>: rand and srand, bsearch, getenv and system.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stddef.h>
#include <stdlib.h>

int acc_rt_mos(int fn, int hl, int de, int bc);

/* The generator C99 7.20.2.2 gives as its example, with RAND_MAX 32767:
 * one multiplication and one addition a number, seeded with 1 until srand
 * says otherwise. */
static unsigned long next = 1;

int rand(void)
{
    next = next * 1103515245UL + 12345UL;

    return (int) ((next >> 16) & 32767);
}

void srand(unsigned seed)
{
    next = seed;
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
