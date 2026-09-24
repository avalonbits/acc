/*
 * wchar.h -- wide characters and multibyte ones (C99 7.24).
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * A wchar_t is sixteen bits, as agondev has it, and holds a character of
 * Unicode's first plane. The multibyte characters of the C locale -- the
 * only one there is -- are UTF-8, which is what acc already makes of a
 * \u in a narrow string literal: so a character is one byte to three, and
 * a four-byte sequence, being past what a wchar_t holds, is not one.
 *
 * The functions that take a long double are not here, since acc has none;
 * wcstold is the one this leaves out.
 */
#ifndef ACC_WCHAR_H
#define ACC_WCHAR_H

#include <stdarg.h>
#include <stddef.h>

#ifndef ACC_WINT_T
#define ACC_WINT_T
typedef int wint_t;
#endif

#ifndef WEOF
#define WEOF ((wint_t) -1)
#endif

#ifndef WCHAR_MIN               /* <stdint.h> has them too, spelled the same */
#define WCHAR_MIN       (-32768)
#define WCHAR_MAX       32767
#endif

/* Where a conversion has got to in a character: its value so far, and how
 * many more bytes it wants. */
typedef struct {
    unsigned short value;
    unsigned char  need;
    unsigned char  min;         /* the least it may come to, against overlongs */
} mbstate_t;

struct __acc_file;

/* Formatted. */
int fwprintf(struct __acc_file *f, const wchar_t *fmt, ...);
int fwscanf(struct __acc_file *f, const wchar_t *fmt, ...);
int swprintf(wchar_t *s, size_t n, const wchar_t *fmt, ...);
int swscanf(const wchar_t *s, const wchar_t *fmt, ...);
int vfwprintf(struct __acc_file *f, const wchar_t *fmt, va_list ap);
int vfwscanf(struct __acc_file *f, const wchar_t *fmt, va_list ap);
int vswprintf(wchar_t *s, size_t n, const wchar_t *fmt, va_list ap);
int vswscanf(const wchar_t *s, const wchar_t *fmt, va_list ap);
int vwprintf(const wchar_t *fmt, va_list ap);
int vwscanf(const wchar_t *fmt, va_list ap);
int wprintf(const wchar_t *fmt, ...);
int wscanf(const wchar_t *fmt, ...);

/* A character at a time. */
wint_t   fgetwc(struct __acc_file *f);
wchar_t *fgetws(wchar_t *s, int n, struct __acc_file *f);
wint_t   fputwc(wchar_t c, struct __acc_file *f);
int      fputws(const wchar_t *s, struct __acc_file *f);
int      fwide(struct __acc_file *f, int mode);
wint_t   getwc(struct __acc_file *f);
wint_t   getwchar(void);
wint_t   putwc(wchar_t c, struct __acc_file *f);
wint_t   putwchar(wchar_t c);
wint_t   ungetwc(wint_t c, struct __acc_file *f);

/* Numbers. */
double             wcstod(const wchar_t *s, wchar_t **end);
float              wcstof(const wchar_t *s, wchar_t **end);
long               wcstol(const wchar_t *s, wchar_t **end, int base);
long long          wcstoll(const wchar_t *s, wchar_t **end, int base);
unsigned long      wcstoul(const wchar_t *s, wchar_t **end, int base);
unsigned long long wcstoull(const wchar_t *s, wchar_t **end, int base);

/* Strings. */
wchar_t *wcscpy(wchar_t *to, const wchar_t *from);
wchar_t *wcsncpy(wchar_t *to, const wchar_t *from, size_t n);
wchar_t *wmemcpy(wchar_t *to, const wchar_t *from, size_t n);
wchar_t *wmemmove(wchar_t *to, const wchar_t *from, size_t n);
wchar_t *wcscat(wchar_t *to, const wchar_t *from);
wchar_t *wcsncat(wchar_t *to, const wchar_t *from, size_t n);
int      wcscmp(const wchar_t *a, const wchar_t *b);
int      wcscoll(const wchar_t *a, const wchar_t *b);
int      wcsncmp(const wchar_t *a, const wchar_t *b, size_t n);
size_t   wcsxfrm(wchar_t *to, const wchar_t *from, size_t n);
int      wmemcmp(const wchar_t *a, const wchar_t *b, size_t n);
wchar_t *wcschr(const wchar_t *s, wchar_t c);
size_t   wcscspn(const wchar_t *s, const wchar_t *reject);
wchar_t *wcspbrk(const wchar_t *s, const wchar_t *accept);
wchar_t *wcsrchr(const wchar_t *s, wchar_t c);
size_t   wcsspn(const wchar_t *s, const wchar_t *accept);
wchar_t *wcsstr(const wchar_t *hay, const wchar_t *needle);
wchar_t *wcstok(wchar_t *s, const wchar_t *delim, wchar_t **save);
wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n);
size_t   wcslen(const wchar_t *s);
wchar_t *wmemset(wchar_t *s, wchar_t c, size_t n);


/* Multibyte and wide, one to the other. */
wint_t btowc(int c);
int    wctob(wint_t c);
int    mbsinit(const mbstate_t *ps);
size_t mbrlen(const char *s, size_t n, mbstate_t *ps);
size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps);
size_t mbsrtowcs(wchar_t *dst, const char **src, size_t len, mbstate_t *ps);
size_t wcsrtombs(char *dst, const wchar_t **src, size_t len, mbstate_t *ps);

#endif
