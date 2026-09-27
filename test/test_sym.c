/*
 * The symbol table, and specifically that finding a file-scope name does not
 * depend on how many there are.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 *
 * This was a linear walk, and the cost of it did not show up as a wrong
 * answer -- it showed up as two inputs identical byte for byte except which
 * of two hundred functions main called taking 7.80 s and 8.76 s to compile.
 * Nothing a differential test can see. What can be seen is the work: how many
 * table slots a lookup touches, and whether that number grows with the table.
 *
 * The names are real ones, interned: a file-scope symbol is found through
 * the name arena, so a NameRef made up without one would find nothing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"

extern unsigned long sym_probes;

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

void acc_error_pos(int line, int col, const char *fmt, ...)
{
    (void) line; (void) col; (void) fmt;
    fprintf(stderr, "  FAIL  acc_error_pos during the test\n");
    exit(2);
}

#define N 500

static NameRef names[N];

/* Probes for one pass of lookups over every name, in hundredths per lookup. */
static long probes_per_lookup(int n)
{
    unsigned long before = sym_probes;
    int i;

    for (i = 0; i < n; i++)
        if (sym_find(names[i]) == SYM_NONE) {
            fprintf(stderr, "  FAIL lookup %d came back empty\n", i);
            failures++;
        }

    return (long) ((sym_probes - before) * 100 / (unsigned long) n);
}

int main(void)
{
    long small, large;
    int i, wrong = 0;

    name_init();
    sym_init();

    for (i = 0; i < N; i++) {
        char text[16];

        sprintf(text, "fn%d", i);
        names[i] = name_intern(text, (int) strlen(text));
    }

    for (i = 0; i < 20; i++)
        sym_push(names[i], SYM_FUNC, 1000 + i);
    small = probes_per_lookup(20);

    for (; i < N; i++)
        sym_push(names[i], SYM_FUNC, 1000 + i);
    large = probes_per_lookup(N);

    fprintf(stderr, "  ---  %ld.%02ld probes per lookup at 20 names, "
                    "%ld.%02ld at %d\n",
            small / 100, small % 100, large / 100, large % 100, N);

    /* The point of the whole thing: twenty-five times as many names must not
     * cost measurably more per lookup. A walk would be twenty-five times
     * worse; 2.00 is loose enough for the probe sequence and tight enough
     * that a walk cannot pass. */
    is("lookup does not grow with the table", large < 200, 1);

    for (i = 0; i < N; i++) {
        int s = sym_find(names[i]);

        if (s == SYM_NONE || sym_at(s)->val != 1000 + i)
            wrong++;
    }
    is("every name finds its own symbol", wrong, 0);
    is("a name never pushed is not found",
       sym_find(name_intern("never_pushed", 12)), SYM_NONE);

    /* A local shadows a file-scope name of the same spelling, and stops doing
     * so when the function ends. */
    sym_push(names[7], SYM_LOCAL, -6);
    is("a local shadows file scope", sym_at(sym_find(names[7]))->val, -6);
    sym_drop_locals();
    is("and stops when the function ends", sym_at(sym_find(names[7]))->val, 1007);

    if (failures)
        fprintf(stderr, "  %d failed\n", failures);
    else
        fprintf(stderr, "  %d passed, 0 failed\n", checks);

    return failures ? 1 : 0;
}
