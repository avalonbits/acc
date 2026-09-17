/*
 * The output image.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"

/* MOS loads a program at 0x040000 and enters it at 0x040045, past a 64-byte
 * header: a jump over itself, a 60-byte name field, then the signature and a
 * byte saying the program runs in ADL mode. There is no loader beyond that --
 * no relocation, no sections -- so what is written here is what runs. */
#define LOAD_ADDR    0x040000
#define HEADER_SIZE  0x45

/* Held in memory and written at the end, because a call to a function defined
 * further down the file has to be patched once its address is known. The
 * image is small -- the previous compiler's output for four hundred lines of
 * input was 33 KB against 206 KB of room -- and it is not what runs the
 * machine out of memory. */
static unsigned char *img;
static int            len, cap;
static const char    *out_path;

void out_open(const char *path)
{
    static const unsigned char hdr[HEADER_SIZE - 4] = { 0 };
    const char *base, *q;

    cap = 4096;
    img = malloc(cap);
    if (!img)
        acc_error("out of memory for the output");
    len = 0;
    out_path = path;

    /* jp 0x040045, over the header */
    out_byte(0xc3);
    out_word24(LOAD_ADDR + HEADER_SIZE);

    memcpy(img + len, hdr, sizeof hdr);
    len += (int) sizeof hdr;

    /* The name MOS lists the program under, which is the file it is written
     * to. agondev has a separate step for this; there is no reason for one. */
    base = path;
    for (q = path; *q; q++)
        if (*q == '/' || *q == '\\')
            base = q + 1;
    {
        size_t n = strlen(base);

        if (n > 0x40 - 4 - 1)
            n = 0x40 - 4 - 1;
        memcpy(img + 4, base, n);
    }
    img[0x40] = 'M';
    img[0x41] = 'O';
    img[0x42] = 'S';
    img[0x43] = 0;        /* header version */
    img[0x44] = 1;        /* ADL, 24-bit addressing */
}

/* Kept out of out_byte, which is called once per byte of the program being
 * compiled. Inline, its `cap *= 2` and its error string need a frame slot, so
 * out_byte opened a three-byte frame on every byte emitted to serve a path
 * taken a dozen times in a compile. Out of line, the hot path is a compare and
 * a store. */
/* The attribute is load-bearing: out_grow is called from one place, so the
 * compiler puts it straight back inline unless told not to. */
__attribute__((noinline))
static void out_grow(void)
{
    /* One doubling is always enough: cap starts at 4096 and the largest
     * single write is the three bytes of out_word24. */
    cap *= 2;
    img = realloc(img, cap);
    if (!img)
        acc_error("out of memory for the output");
}

void out_byte(int b)
{
    if (len == cap)
        out_grow();
    img[len++] = (unsigned char) b;
}

void out_word24(int v)
{
    /* One bounds check and three stores, rather than three calls that each
     * check. Every call instruction and every loaded constant emits one of
     * these, so it is a third of the output path. */
    if (len + 3 > cap)
        out_grow();
    img[len]     = (unsigned char) v;
    img[len + 1] = (unsigned char) (v >> 8);
    img[len + 2] = (unsigned char) (v >> 16);
    len += 3;
}

int out_here(void)
{
    return LOAD_ADDR + len;
}

void out_patch24(int at, int v)
{
    int off = at - LOAD_ADDR;

    if (off < 0 || off + 3 > len)
        acc_error("internal: patch at %06x is outside the image", at);
    img[off]     = (unsigned char) v;
    img[off + 1] = (unsigned char) (v >> 8);
    img[off + 2] = (unsigned char) (v >> 16);
}

void out_close(void)
{
    FILE *f = fopen(out_path, "wb");

    if (!f)
        acc_error("cannot write '%s'", out_path);
    if ((int) fwrite(img, 1, len, f) != len)
        acc_error("short write on '%s'", out_path);
    fclose(f);
    free(img);
    img = NULL;
}
