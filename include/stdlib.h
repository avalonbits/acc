/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef ACC_STDLIB_H
#define ACC_STDLIB_H

#include <stddef.h>

void *malloc(size_t n);
void *calloc(size_t count, size_t size);
void *realloc(void *p, size_t n);
void  free(void *p);

int   abs(int n);
long  labs(long n);
int   atoi(const char *s);
long  atol(const char *s);

#endif
