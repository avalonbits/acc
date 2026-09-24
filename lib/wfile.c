/*
 * Wide characters on streams: each is its UTF-8 bytes in the stream, read
 * and written through the byte functions.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * A stream takes the orientation of the first thing done to it, as C99
 * 7.19.2 says, and fwide reports it. A wide character pushed back is kept
 * as itself, beside the byte ungetc keeps, since its bytes may be more than
 * the one C promises to push back.
 */
#include <errno.h>
#include <stdio.h>
#include <wchar.h>

static void wide(FILE *f)
{
    if (f && !f->orient)
        f->orient = 1;
}

int fwide(FILE *f, int mode)
{
    if (!f)
        return 0;
    if (!f->orient && mode)
        f->orient = mode > 0 ? 1 : -1;

    return f->orient;
}

wint_t fgetwc(FILE *f)
{
    mbstate_t st = { 0, 0, 0 };
    wchar_t wc;
    char b;
    int c;
    size_t r;

    wide(f);
    if (!f)
        return WEOF;
    if (f->wunget != EOF) {
        wc = (wchar_t) f->wunget;
        f->wunget = EOF;

        return (wint_t) (unsigned short) wc;
    }
    do {
        c = fgetc(f);
        if (c == EOF) {
            if (!mbsinit(&st)) {
                f->error = 1;
                errno = EILSEQ;
            }

            return WEOF;
        }
        b = (char) c;
        r = mbrtowc(&wc, &b, 1, &st);
    } while (r == (size_t) -2);
    if (r == (size_t) -1) {
        f->error = 1;

        return WEOF;
    }

    return (wint_t) (unsigned short) wc;
}

wint_t getwc(FILE *f)
{
    return fgetwc(f);
}

wint_t getwchar(void)
{
    return fgetwc(stdin);
}

wint_t ungetwc(wint_t c, FILE *f)
{
    if (!f || c == WEOF || f->wunget != EOF)
        return WEOF;
    wide(f);
    f->wunget = c;
    f->eof = 0;

    return c;
}

wchar_t *fgetws(wchar_t *s, int n, FILE *f)
{
    wchar_t *p = s;
    wint_t c = 0;

    if (n <= 0)
        return NULL;
    while (--n > 0 && (c = fgetwc(f)) != WEOF) {
        *p++ = (wchar_t) c;
        if (c == L'\n')
            break;
    }
    if (p == s || (c == WEOF && ferror(f)))
        return NULL;
    *p = 0;

    return s;
}

wint_t fputwc(wchar_t c, FILE *f)
{
    char b[3];
    size_t n, i;

    wide(f);
    n = wcrtomb(b, c, NULL);
    if (n == (size_t) -1) {
        if (f)
            f->error = 1;

        return WEOF;
    }
    for (i = 0; i < n; i++)
        if (fputc((unsigned char) b[i], f) == EOF)
            return WEOF;

    return (wint_t) (unsigned short) c;
}

wint_t putwc(wchar_t c, FILE *f)
{
    return fputwc(c, f);
}

wint_t putwchar(wchar_t c)
{
    return fputwc(c, stdout);
}

int fputws(const wchar_t *s, FILE *f)
{
    for (; *s; s++)
        if (fputwc(*s, f) == WEOF)
            return EOF;

    return 0;
}
