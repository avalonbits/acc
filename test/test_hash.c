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

/* lex.c reports through these; nothing here should reach one. */
void acc_error(const char *fmt, ...)
{
    (void) fmt;
    fprintf(stderr, "  FAIL  acc_error during the test\n");
    exit(2);
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
     * when the hash is good; 3 is loose enough not to be brittle and tight
     * enough that halving the hash's width fails it. */
    {
        unsigned long before = name_probes, probes;

        for (i = 0; i < N; i++)
            name_intern(text[i], (int) strlen(text[i]));
        probes = name_probes - before;

        fprintf(stderr, "  ---  %lu probes for %d lookups (%lu.%02lu each)\n",
                probes, N, probes / N, probes * 100 / N % 100);
        is("probes per lookup, times 100", (long) (probes * 100 / N) < 300, 1);
    }

    /* Nothing may land on the reserved offset, which is what means "none". */
    for (i = 0; i < N; i++)
        if (refs[i] == NAME_NONE)
            failures++;
    is("none is still none", (long) NAME_NONE, 0);

    if (failures)
        fprintf(stderr, "  %d failed\n", failures);
    else
        fprintf(stderr, "  %d passed, 0 failed\n", checks);

    return failures ? 1 : 0;
}
