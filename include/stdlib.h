/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef ACC_STDLIB_H
#define ACC_STDLIB_H

#include <stddef.h>

void *malloc(size_t n);
void *calloc(size_t count, size_t size);
void *realloc(void *p, size_t n);
void  free(void *p);

void  qsort(void *base, size_t nmemb, size_t size,
            int (*cmp)(const void *, const void *));

/* Neither returns. exit is emitted at the call rather than called, which
 * is why the library has no member for it; abort is a member, and is the
 * one line that gives exit a status saying the program went wrong. */
__attribute__((noreturn)) void exit(int status);
__attribute__((noreturn)) void abort(void);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

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

/* Past the type's range, these answer its limit and put ERANGE in errno. */
long               strtol(const char *s, char **end, int base);
unsigned long      strtoul(const char *s, char **end, int base);
long long          strtoll(const char *s, char **end, int base);
unsigned long long strtoull(const char *s, char **end, int base);

#endif
