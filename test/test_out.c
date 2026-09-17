/*
 * The output image, across the points where its buffer grows.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 *
 * out_word24 writes three bytes after one bounds check rather than calling
 * out_byte three times. That moves the growth decision from "is there room
 * for one byte" to "is there room for three", and the interesting inputs are
 * the ones where a word straddles the old capacity -- which the differential
 * tests reach only by accident, and which they would report as a program that
 * crashed rather than as an image that was cut short.
 *
 * So: write enough to cross several doublings, read the file back, and check
 * every byte of it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"

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

#define WORDS 20000      /* 60 KB: past cap 4096 doubling four times over */

int main(void)
{
    const char *path = "bin/.test_out.bin";
    unsigned char *want;
    unsigned char *got;
    long n;
    int i, base, wrong = 0;
    FILE *f;

    out_open(path);
    base = out_here();

    /* A value whose three bytes all differ, and differ between words, so a
     * byte written in the wrong place or dropped at a boundary shows up. */
    for (i = 0; i < WORDS; i++)
        out_word24(i * 7 + (i << 12));

    /* Single bytes after the words, to check len is still where it should be
     * once out_word24 has been the one advancing it. */
    for (i = 0; i < 300; i++)
        out_byte(i);

    is("out_here tracks what was written", out_here() - base, WORDS * 3 + 300);

    /* A patch into the middle, which reads len and must still see all of it. */
    out_patch24(base + 3 * (WORDS / 2), 0xabcdef);

    out_close();

    f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "  FAIL cannot read back %s\n", path); return 1; }
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);

    got = malloc((size_t) n);
    if (fread(got, 1, (size_t) n, f) != (size_t) n) { fprintf(stderr, "  FAIL short read\n"); return 1; }
    fclose(f);
    remove(path);

    is("file length", n, (base - 0x040000) + WORDS * 3 + 300);

    want = malloc((size_t) n);
    memcpy(want, got, (size_t) n);
    for (i = 0; i < WORDS; i++) {
        int v = (i == WORDS / 2) ? 0xabcdef : (i * 7 + (i << 12));
        int at = (base - 0x040000) + i * 3;

        want[at]     = (unsigned char) v;
        want[at + 1] = (unsigned char) (v >> 8);
        want[at + 2] = (unsigned char) (v >> 16);
    }
    for (i = 0; i < 300; i++)
        want[(base - 0x040000) + WORDS * 3 + i] = (unsigned char) i;

    for (i = base - 0x040000; i < n; i++)
        if (got[i] != want[i]) {
            if (wrong == 0)
                fprintf(stderr, "  ---  first wrong byte at %d: %02x, want %02x\n",
                        i, got[i], want[i]);
            wrong++;
        }
    is("every byte written is in the file", wrong, 0);

    free(got); free(want);

    if (failures)
        fprintf(stderr, "  %d failed\n", failures);
    else
        fprintf(stderr, "  %d passed, 0 failed\n", checks);

    return failures ? 1 : 0;
}
