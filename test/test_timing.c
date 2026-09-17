/*
 * elapsed_cs, against known clock values.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 *
 * Checked against numbers rather than against how long a compile takes, which
 * would make the test depend on the machine running it. CLOCKS_PER_SEC is
 * 1000000 here and 100 on the Agon, so the arithmetic is written in terms of
 * it and the same test covers both.
 */

#include <stdio.h>
#include <time.h>

#include "timing.h"

static int failures = 0;

static void is(const char *name, unsigned got, unsigned want)
{
    if (got == want) {
        fprintf(stderr, "  ok   %-34s %u\n", name, got);
    } else {
        fprintf(stderr, "  FAIL %-34s got %u, want %u\n", name, got, want);
        failures++;
    }
}

int main(void)
{
    const clock_t sec = (clock_t) CLOCKS_PER_SEC;
    const clock_t cs  = (clock_t) (CLOCKS_PER_SEC / 100);

    is("no time at all", elapsed_cs(0, 0), 0);
    is("one centisecond", elapsed_cs(0, cs), 1);
    is("one second is a hundred", elapsed_cs(0, sec), 100);
    is("two seconds", elapsed_cs(0, 2 * sec), 200);
    is("nineteen hundredths", elapsed_cs(0, 19 * cs), 19);

    /* The reading is a difference, so a clock that did not start at zero has
     * to give the same answer. A compile is never the first thing to run. */
    is("from a non-zero start", elapsed_cs(sec, 2 * sec), 100);
    is("and from an arbitrary one", elapsed_cs(7 * cs, 26 * cs), 19);

    /* Shorter than a hundredth reads as zero, which is why a small file
     * compiles in "0.00 seconds" rather than in some made-up figure. */
    is("just under a centisecond", elapsed_cs(0, cs - 1), 0);
    is("just over one", elapsed_cs(0, cs + 1), 1);

    /* The split main prints as "%u.%02u" has to read back as the same number
     * of seconds and hundredths. */
    {
        const unsigned t = elapsed_cs(0, 19 * cs);
        is("printed whole seconds", t / 100, 0);
        is("printed hundredths", t % 100, 19);

        const unsigned u = elapsed_cs(0, 3 * sec + 7 * cs);
        is("over a second, whole seconds", u / 100, 3);
        is("over a second, hundredths", u % 100, 7);
    }

    if (failures)
        fprintf(stderr, "  %d failed\n", failures);
    else
        fprintf(stderr, "  13 passed, 0 failed\n");

    return failures ? 1 : 0;
}
