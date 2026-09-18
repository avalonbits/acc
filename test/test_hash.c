/*
 * The name arena, which is to say the hash under it.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 *
 * The differential tests say names still resolve, but they compile programs
 * with a few hundred names in them and would not notice a hash that had
 * quietly become a linear list. What is checked here is the thing the hash has
 * to do -- give every distinct name a distinct ref, give the same name the
 * same ref every time -- and, separately, that it still spreads.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "acc.h"

extern unsigned long name_probes;

static int failures = 0;
static int checks = 0;

static void is(const char *name, long got, long want)
{
    checks++;
    if (got == want) {
        fprintf(stderr, "  ok   %-34s %ld\n", name, got);
    } else {
        fprintf(stderr, "  FAIL %-34s got %ld, want %ld\n", name, got, want);
        failures++;
    }
}

/* Set by the last check, which is the only one that means to reach
 * acc_error: the name that does not fit. */
static int  at_limit, interned;

static void finish(void);

/* lex.c reports through these; nothing here should reach one but that. */
void acc_error(const char *fmt, ...)
{
    (void) fmt;
    if (!at_limit) {
        fprintf(stderr, "  FAIL  acc_error during the test\n");
        exit(2);
    }
    is("refused at name number", interned, 8192);
    finish();
}

void acc_error_at(int line, const char *fmt, ...)
{
    (void) line; (void) fmt;
    fprintf(stderr, "  FAIL  acc_error_at during the test\n");
    exit(2);
}

#define N 2000

static NameRef refs[N];
static char    text[N][16];

int main(void)
{
    int i, j, dup = 0, stable = 0, roundtrip = 0;

    name_init();

    /* A lookup compares against every name its probe passes, and those can be
     * shorter than the name looked up. The comparison has to stop at the
     * stored name's end: reading on for the full length of the other runs
     * off the arena when the shorter one is the last thing in it. So: 127
     * names of eight bytes and one of seven fill the first 1024 bytes of the
     * arena exactly, which is its first allocation, and the long name after
     * them is one whose probe passes through the end -- found by search, and
     * the probe count below says whether it still does. Under the address
     * sanitizer the over-read is a crash. */
    {
        char fill[16];
        unsigned long before;

        for (i = 0; i < 127; i++) {
            sprintf(fill, "f%06d", i);
            name_intern(fill, 7);
        }
        is("the short name ends the arena", (long) name_intern("s00000", 6) + 7, 1024);
        before = name_probes;
        name_intern("a_longer_name_73", 16);
        is("and a longer one's probe passes it", name_probes - before >= 2, 1);
        interned = 129;
    }

    /* Names of the shape a program actually has: a short prefix and a number,
     * so most of them differ only in their last character or two. A hash that
     * leans on the first bytes looks fine on English words and collapses
     * here. */
    for (i = 0; i < N; i++) {
        sprintf(text[i], "%s%d", (i & 1) ? "f" : "var_", i);
        refs[i] = name_intern(text[i], (int) strlen(text[i]));
    }

    for (i = 0; i < N; i++) {
        if (strcmp(name_text(refs[i]), text[i]) == 0)
            roundtrip++;
        if (name_intern(text[i], (int) strlen(text[i])) == refs[i])
            stable++;
    }
    is("every name reads back", roundtrip, N);
    is("interning twice gives one ref", stable, N);

    /* Distinct names must not share a ref. O(n^2), and n is small on purpose:
     * the point is to be exhaustive rather than quick. */
    for (i = 0; i < N; i++)
        for (j = i + 1; j < N; j++)
            if (refs[i] == refs[j])
                dup++;
    is("no two names share a ref", dup, 0);

    /* The hash has to spread, not just resolve. Open addressing gives correct
     * answers however bad the hash is -- a constant one degenerates the table
     * into a linear list and every test above still passes -- so what is
     * checked is the work done: probes per lookup over the second, all-hits
     * pass. A table kept under half full averages about 1.5 probes for a hit
     * when the hash is good.
     *
     * The bound is 1.8 rather than a round 3. Two independent Pearson lanes
     * gave 1.54 and the single-lane version that replaced them gives 1.50,
     * but the first attempt at halving the work -- chaining, where the second
     * byte is just the previous value of the first -- gave exactly 2.00,
     * because two bytes one step apart in the same chain are not independent
     * enough to spread. A bound of 3 accepts that; this one does not, while
     * still leaving room above both hashes that work. */
    {
        unsigned long before = name_probes, probes;

        for (i = 0; i < N; i++)
            name_intern(text[i], (int) strlen(text[i]));
        probes = name_probes - before;

        fprintf(stderr, "  ---  %lu probes for %d lookups (%lu.%02lu each)\n",
                probes, N, probes / N, probes * 100 / N % 100);
        is("probes per lookup, times 100", (long) (probes * 100 / N) < 180, 1);
    }

    /* Nothing may land on the reserved offset, which is what means "none". */
    for (i = 0; i < N; i++)
        if (refs[i] == NAME_NONE)
            failures++;
    is("none is still none", (long) NAME_NONE, 0);

    /* The table is 64 KB at most, because a probe's starting offset is put
     * together from two bytes: 16384 slots, kept under half full, is 8191
     * names. The one after them has to be refused rather than hashed into
     * slots the offset cannot reach -- a table that kept growing past that
     * would still answer, and answer wrongly. */
    {
        char more[16];

        int left;

        interned += N;
        left = 8191 - interned;
        at_limit = 1;
        for (i = 0; i < left; i++) {
            sprintf(more, "g%d", i);
            name_intern(more, (int) strlen(more));
            interned++;
        }
        interned++;
        name_intern("one_too_many", 12);
        is("the name past the limit is refused", 0, 1);
    }

    finish();

    return 0;
}

static void finish(void)
{
    if (failures)
        fprintf(stderr, "  %d failed\n", failures);
    else
        fprintf(stderr, "  %d passed, 0 failed\n", checks);

    exit(failures ? 1 : 0);
}
