/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#ifndef ACC_STDLIB_H
#define ACC_STDLIB_H

/* The names of <stddef.h> this header is given (C99 7.1.3), and no others:
 * a program that includes it may define ptrdiff_t or offsetof itself. */
#ifndef ACC_SIZE_T
#define ACC_SIZE_T
typedef unsigned int size_t;
#endif
#ifndef ACC_WCHAR_T
#define ACC_WCHAR_T
typedef short wchar_t;
#endif
#ifndef NULL
#define NULL ((void *) 0)
#endif

void *malloc(size_t n);
void *calloc(size_t count, size_t size);
void *realloc(void *p, size_t n);
void  free(void *p);

void  qsort(void *base, size_t nmemb, size_t size,
            int (*cmp)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*cmp)(const void *, const void *));

#define RAND_MAX 32767

int   rand(void);
void  srand(unsigned seed);

char *getenv(const char *name);
int   system(const char *command);

/* None of these returns. exit is emitted at the call rather than called;
 * the end of the program then runs what atexit registered and closes the
 * files, through the startup stub's exit hook. _Exit and abort skip that. */
__attribute__((noreturn)) void exit(int status);
__attribute__((noreturn)) void _Exit(int status);
__attribute__((noreturn)) void abort(void);
int atexit(void (*f)(void));

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* The most bytes a character takes: UTF-8, as far as a sixteen-bit
 * wchar_t goes. See lib/mb.c. */
#define MB_CUR_MAX   3

int    mblen(const char *s, size_t n);
int    mbtowc(wchar_t *pwc, const char *s, size_t n);
int    wctomb(char *s, wchar_t wc);
size_t mbstowcs(wchar_t *dst, const char *src, size_t n);
size_t wcstombs(char *dst, const wchar_t *src, size_t n);

typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;
typedef struct { long long quot, rem; } lldiv_t;

int       abs(int n);
long      labs(long n);
long long llabs(long long n);
div_t     div(int a, int b);
ldiv_t    ldiv(long a, long b);
lldiv_t   lldiv(long long a, long long b);

int       atoi(const char *s);
long      atol(const char *s);
long long atoll(const char *s);

double    atof(const char *s);
double    strtod(const char *s, char **end);
float     strtof(const char *s, char **end);

/* Past the type's range, these answer its limit and put ERANGE in errno. */
long               strtol(const char *s, char **end, int base);
unsigned long      strtoul(const char *s, char **end, int base);
long long          strtoll(const char *s, char **end, int base);
unsigned long long strtoull(const char *s, char **end, int base);

#endif
