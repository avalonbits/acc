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
/* Held as a write pointer and the end of the buffer rather than as an index
 * and a capacity. Every byte the compiler emits comes through out_byte, and
 * `img[len++]` is an addition of two 24-bit values before the store, where a
 * pointer that walks is a store and an increment. The bound is the same
 * either way: one compare, against the end instead of against the size. */
static unsigned char *img;
static unsigned char *put;              /* where the next byte goes */
static unsigned char *limit;            /* one past the last it may use */
static int            cap;
static const char    *out_path;

#define OUT_LEN ((int) (put - img))

void out_open(const char *path)
{
    static const unsigned char hdr[HEADER_SIZE - 4] = { 0 };
    const char *base, *scan;

    cap = 4096;
    img = malloc(cap);
    if (!img)
        acc_error("out of memory for the output");
    put = img;
    limit = img + cap;
    out_path = path;

    /* jp 0x040045, over the header */
    out_byte(0xc3);
    out_word24(LOAD_ADDR + HEADER_SIZE);

    memcpy(put, hdr, sizeof hdr);
    put += sizeof hdr;

    /* The name MOS lists the program under, which is the file it is written
     * to. agondev has a separate step for this; there is no reason for one. */
    base = path;
    for (scan = path; *scan; scan++)
        if (*scan == '/' || *scan == '\\')
            base = scan + 1;
    {
        size_t name_len = strlen(base);

        if (name_len > 0x40 - 4 - 1)
            name_len = 0x40 - 4 - 1;
        memcpy(img + 4, base, name_len);
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
    int used = OUT_LEN;

    /* One doubling is always enough: cap starts at 4096 and the largest
     * single write is the three bytes of out_word24. */
    cap *= 2;
    img = realloc(img, cap);
    if (!img)
        acc_error("out of memory for the output");
    put = img + used;
    limit = img + cap;
}

void out_byte(int byte)
{
    if (put == limit)
        out_grow();
    *put++ = (unsigned char) byte;
}

/* Two bytes and three bytes, which is what nearly every instruction acc emits
 * is made of: a prefix and an opcode, or those and a displacement.
 *
 * Written as repeated out_byte calls they cost a call and a bounds check
 * each. The check is the same one either way -- the buffer either has room
 * for the whole instruction or it does not -- and the call is pure overhead
 * on the path that runs once per byte of every program compiled. */
void out_byte2(int first, int second)
{
    if (limit - put < 2)
        out_grow();
    put[0] = (unsigned char) first;
    put[1] = (unsigned char) second;
    put += 2;
}

void out_byte3(int first, int second, int third)
{
    if (limit - put < 3)
        out_grow();
    put[0] = (unsigned char) first;
    put[1] = (unsigned char) second;
    put[2] = (unsigned char) third;
    put += 3;
}

void out_word24(int value)
{
    /* One bounds check and three stores, rather than three calls that each
     * check. Every call instruction and every loaded constant emits one of
     * these, so it is a third of the output path. */
    if (limit - put < 3)
        out_grow();
    put[0] = (unsigned char) value;
    put[1] = (unsigned char) (value >> 8);
    put[2] = (unsigned char) (value >> 16);
    put += 3;
}

int out_here(void)
{
    return LOAD_ADDR + OUT_LEN;
}

void out_patch24(int at, int value)
{
    int off = at - LOAD_ADDR;

    if (off < 0 || off + 3 > OUT_LEN)
        acc_error("internal: patch at %06x is outside the image", at);
    img[off]     = (unsigned char) value;
    img[off + 1] = (unsigned char) (value >> 8);
    img[off + 2] = (unsigned char) (value >> 16);
}

void out_close(void)
{
    FILE *file = fopen(out_path, "wb");

    if (!file)
        acc_error("cannot write '%s'", out_path);
    if ((int) fwrite(img, 1, (size_t) OUT_LEN, file) != OUT_LEN)
        acc_error("short write on '%s'", out_path);
    fclose(file);
    free(img);
    img = NULL;
}
