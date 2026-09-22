/*
 * Files, over the calls MOS already has.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * MOS opens, reads, writes, closes and seeks; what C asks for on top of that
 * is a handle that remembers which way it is going and a buffer so that a
 * program writing three bytes at a time does not write to the card three
 * bytes at a time. An object file is mostly three-byte numbers -- every
 * symbol, every relocation -- so that buffer is the difference between
 * writing one and waiting for one.
 *
 * Reads are not buffered. What reads here reads whole files: acc takes a
 * source or an object in one call and works on it in memory, so a buffer
 * would be a second copy of what the caller already asked to have.
 *
 * The table is fixed rather than allocated. A program has a few files open
 * at once and never many, and the heap on this machine is worth more to
 * whatever the program is actually doing.
 */
#include <stdio.h>
#include <string.h>

#include <agon/mos.h>

static FILE files[FOPEN_MAX];
static FILE screen_out = { 0, 1, 1, 0, 0, { 0 } };
static FILE screen_err = { 0, 1, 1, 0, 0, { 0 } };

FILE *stdout = &screen_out;
FILE *stderr = &screen_err;

/* Everything held, handed to MOS. A file that is not being written to has
 * nothing held and this is the walk that says so. */
int fflush(FILE *f)
{
    int n;

    if (!f || !f->held)
        return 0;
    if (f->screen) {
        f->held = 0;

        return 0;
    }
    n = (int) mos_fwrite(f->fh, f->buf, (uint24_t) f->held);
    if (n != f->held) {
        f->error = 1;
        f->held = 0;

        return EOF;
    }
    f->held = 0;

    return 0;
}

FILE *fopen(const char *path, const char *mode)
{
    int writing = mode[0] == 'w' || mode[0] == 'a';
    uint8_t flags, fh;
    int i;

    if (mode[0] != 'r' && !writing)
        return NULL;
    for (i = 0; i < FOPEN_MAX; i++)
        if (!files[i].fh)
            break;
    if (i == FOPEN_MAX)
        return NULL;

    flags = writing ? (uint8_t) (FA_WRITE | FA_CREATE_ALWAYS)
                    : (uint8_t) (FA_READ | FA_OPEN_EXISTING);
    fh = mos_fopen(path, flags);
    if (!fh)
        return NULL;

    files[i].fh = fh;
    files[i].screen = 0;
    files[i].writing = (unsigned char) writing;
    files[i].error = 0;
    files[i].held = 0;

    return &files[i];
}

int fclose(FILE *f)
{
    int bad;

    if (!f || f->screen)
        return 0;
    bad = fflush(f) != 0;
    mos_fclose(f->fh);
    f->fh = 0;                  /* the slot is free again */

    return bad ? EOF : 0;
}

size_t fread(void *to, size_t size, size_t count, FILE *f)
{
    uint24_t want = (uint24_t) (size * count), got;

    if (!f || f->screen || !want)
        return 0;
    got = mos_fread(f->fh, (char *) to, want);

    return size ? (size_t) (got / size) : 0;
}

size_t fwrite(const void *from, size_t size, size_t count, FILE *f)
{
    const char *p = from;
    size_t want = size * count, left = want;

    if (!f || !want)
        return 0;
    if (f->screen) {
        while (left--)
            putchar((unsigned char) *p++);

        return count;
    }

    /* Anything that would not leave room goes straight out: a big write has
     * nothing to gain by being copied into a buffer first. */
    if (left >= (size_t) BUFSIZ) {
        if (fflush(f) != 0)
            return 0;
        if ((size_t) mos_fwrite(f->fh, (char *) from, (uint24_t) left) != left) {
            f->error = 1;

            return 0;
        }

        return count;
    }

    if (f->held + (int) left > BUFSIZ && fflush(f) != 0)
        return 0;
    memcpy(f->buf + f->held, p, left);
    f->held += (int) left;

    return count;
}

int fputc(int c, FILE *f)
{
    char b = (char) c;

    if (!f)
        return EOF;
    if (f->screen)
        return putchar((unsigned char) c);
    if (f->held == BUFSIZ && fflush(f) != 0)
        return EOF;
    f->buf[f->held++] = b;

    return (unsigned char) c;
}

int fputs(const char *s, FILE *f)
{
    size_t n = strlen(s);

    return fwrite(s, 1, n, f) == n ? 0 : EOF;
}

int fgetc(FILE *f)
{
    char b;

    if (!f || f->screen)
        return EOF;
    if (mos_fread(f->fh, &b, 1) != 1)
        return EOF;

    return (unsigned char) b;
}

/* Where MOS has got to, which is where the file is once what is held has
 * been handed over. */
long ftell(FILE *f)
{
    FIL *fil;

    if (!f || f->screen)
        return -1;
    if (fflush(f) != 0)
        return -1;
    fil = mos_getfil(f->fh);

    return fil ? (long) fil->fptr : -1;
}

int fseek(FILE *f, long offset, int whence)
{
    FIL *fil;
    long to;

    if (!f || f->screen)
        return -1;
    if (fflush(f) != 0)
        return -1;
    fil = mos_getfil(f->fh);
    if (!fil)
        return -1;

    /* MOS seeks to a place and not by an amount, so the other two are worked
     * out from what it knows: where the file ends and where it is now. */
    if (whence == SEEK_SET)
        to = offset;
    else if (whence == SEEK_CUR)
        to = (long) fil->fptr + offset;
    else if (whence == SEEK_END)
        to = (long) fil->obj.objsize + offset;
    else
        return -1;
    if (to < 0)
        return -1;

    return mos_flseek(f->fh, (uint32_t) to) == 0 ? 0 : -1;
}

int ferror(FILE *f)
{
    return f ? f->error : 1;
}

/* Printing to a file, which is the formatter in lib/printf.c with this file
 * as where the characters go. The pointing is done here so that printf does
 * not name anything in this file: a program that prints and never opens a
 * file should not carry any of this, and a library member is taken whole. */
extern FILE  *acc_sink_file;
extern int  (*acc_sink_putc)(int, FILE *);
extern char  *acc_sink_buf;
int acc_format(const char *fmt, va_list ap);

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    int n;

    acc_sink_buf = NULL;
    acc_sink_file = f;
    acc_sink_putc = fputc;
    n = acc_format(fmt, ap);
    acc_sink_file = NULL;

    return n;
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vfprintf(f, fmt, ap);
    va_end(ap);

    return n;
}
