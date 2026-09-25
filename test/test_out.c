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
#define PAIRS 5000       /* 25 KB more, so the growth happens again */
#define TRIPLES 40000    /* 120 KB of three-byte writes alone */
#define QUADS 40000      /* and 160 KB of four-byte ones */

/* The relocation table, grown while a function's jumps are merged into it.
 *
 * out_reloc_merge makes room first and merges after, and it once took its
 * pointer into the table before making room: when growing moved the table
 * to a lower address, the stale pointer was above all of it, the merge took
 * nothing from the table, and the jumps went on top, out of order. Whether
 * a growth moves a block, and which way, is the allocator's business, so
 * this test takes it over: linked with --wrap=realloc, the table's first
 * block is the upper half of one array and its second the lower half, which
 * is below it by definition. */
void *__real_realloc(void *p, size_t n);

static int in_scenario;
static int halves[2][4096];

void *__wrap_realloc(void *p, size_t n)
{
    if (!in_scenario)
        return __real_realloc(p, n);
    if (!p)
        return halves[1];
    memcpy(halves[0], p,
           (size_t) (out_reloc_limit - out_relocs) * sizeof *out_relocs);

    return halves[0];
}

static void merge_across_growth(void)
{
    int add[20], i, first, sorted = 1, n;

    in_scenario = 1;
    out_relocs = out_reloc_put = out_reloc_limit = NULL;

    for (i = 0; i < 10; i++)                    /* before the function */
        out_reloc(out_base + 0x100 + i * 3);
    first = out_nrelocs();
    for (i = 0; i < 240; i++)                   /* the function's own slots */
        out_reloc(out_base + 0x1000 + i * 6);
    for (i = 0; i < 20; i++)                    /* its jumps, among them */
        add[i] = 0x1003 + i * 60;

    out_reloc_merge(add, 20, first);            /* 5 slots of room: it grows */
    in_scenario = 0;

    n = out_nrelocs();
    for (i = 1; i < n; i++)
        if (out_reloc_at(i) <= out_reloc_at(i - 1))
            sorted = 0;
    is("the table moved down while merging", out_relocs == halves[0], 1);
    is("merged in order across a growth", sorted, 1);
    is("and nothing lost", n, 270);
}

int main(void)
{
    const char *path = "bin/.test_out.bin";
    unsigned char *want;
    unsigned char *got;
    long n;
    int i, base, wrong = 0;
    FILE *f;

    /* 60 KB without the image growing once: it starts at 64 KB, because on
     * the Agon every growth holds the old copy and the new one at once. */
    out_open(path, 1);
    is("the image starts at 64 KB", out_capacity(), 65536);
    for (i = 0; i < WORDS; i++)
        out_word24(i);
    is("and 60 KB fits it as it is", out_capacity(), 65536);
    out_free();

    /* Everything after starts it small, so that it grows many times over
     * and the boundaries below fall at every alignment. */
    out_start_cap = 4096;
    out_open(path, 1);
    base = out_here();

    /* A value whose three bytes all differ, and differ between words, so a
     * byte written in the wrong place or dropped at a boundary shows up. */
    for (i = 0; i < WORDS; i++)
        out_word24(i * 7 + (i << 12));


    /* Single bytes after the words, to check len is still where it should be
     * once out_word24 has been the one advancing it. */
    for (i = 0; i < 300; i++)
        out_byte(i);

    /* And the two- and three-byte forms, which make the same trade as
     * out_word24 -- one bounds check for the whole instruction instead of one
     * per byte -- and so have the same boundary to get wrong.
     *
     * Three passes with a single byte between them, because one pass is not
     * enough. The buffer doubles from 4096, so where a growth falls relative
     * to a three-byte write depends on how many bytes came before it, and one
     * pass only ever tries one of the three alignments. A check of `< 1`
     * where `< 3` was meant writes two bytes past the end -- and passed,
     * because the one alignment that pass happened to use never left exactly
     * two bytes free. The odd byte between passes shifts it. */
    for (i = 0; i < PAIRS; i++) {
        out_byte2(i, i + 1);
        out_byte3(i + 2, i + 3, i + 4);
    }

    /* Then a long run of nothing but the three-byte form. Mixing widths is
     * not enough on its own: two and three alternating move the buffer on by
     * five at a time, so however many are written the gap left before a
     * growth only ever takes one value, and a check of `< 1` where `< 3` was
     * meant can sit behind that one value writing past the end. Advancing by
     * three alone walks the gap through 2, 1 and 0 over successive doublings,
     * and it is the 2 that catches it. */
    for (i = 0; i < TRIPLES; i++)
        out_byte3(i, i + 1, i + 2);

    /* And the four-byte form the same way, on its own, so that the gap in
     * front of a growth walks 3, 2, 1 and 0 instead of sitting at one value. */
    for (i = 0; i < QUADS; i++)
        out_opcode24(i, i * 7 + (i << 12));

    is("out_here tracks what was written", out_here() - base,
       WORDS * 3 + 300 + PAIRS * 5 + TRIPLES * 3 + QUADS * 4);

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

    is("file length", n, (base - 0x040000)
       + WORDS * 3 + 300 + PAIRS * 5 + TRIPLES * 3 + QUADS * 4);

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
    for (i = 0; i < PAIRS; i++) {
        int at = (base - 0x040000) + WORDS * 3 + 300 + i * 5;

        want[at]     = (unsigned char) i;
        want[at + 1] = (unsigned char) (i + 1);
        want[at + 2] = (unsigned char) (i + 2);
        want[at + 3] = (unsigned char) (i + 3);
        want[at + 4] = (unsigned char) (i + 4);
    }
    for (i = 0; i < TRIPLES; i++) {
        int at = (base - 0x040000) + WORDS * 3 + 300 + PAIRS * 5 + i * 3;

        want[at]     = (unsigned char) i;
        want[at + 1] = (unsigned char) (i + 1);
        want[at + 2] = (unsigned char) (i + 2);
    }
    for (i = 0; i < QUADS; i++) {
        int at = (base - 0x040000)
               + WORDS * 3 + 300 + PAIRS * 5 + TRIPLES * 3 + i * 4;
        int v = i * 7 + (i << 12);

        want[at]     = (unsigned char) i;
        want[at + 1] = (unsigned char) v;
        want[at + 2] = (unsigned char) (v >> 8);
        want[at + 3] = (unsigned char) (v >> 16);
    }

    for (i = base - 0x040000; i < n; i++)
        if (got[i] != want[i]) {
            if (wrong == 0)
                fprintf(stderr, "  ---  first wrong byte at %d: %02x, want %02x\n",
                        i, got[i], want[i]);
            wrong++;
        }
    is("every byte written is in the file", wrong, 0);

    free(got); free(want);

    merge_across_growth();

    if (failures)
        fprintf(stderr, "  %d failed\n", failures);
    else
        fprintf(stderr, "  %d passed, 0 failed\n", checks);

    return failures ? 1 : 0;
}
