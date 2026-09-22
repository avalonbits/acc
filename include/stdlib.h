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

/* exit does not return: acc emits the unwind at the call rather than
 * calling anything, which is why the library has no member for it. */
void  exit(int status);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

int   abs(int n);
long  labs(long n);
int   atoi(const char *s);
long  atol(const char *s);
long  strtol(const char *s, char **end, int base);

#endif
