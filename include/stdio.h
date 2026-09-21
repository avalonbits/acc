/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef ACC_STDIO_H
#define ACC_STDIO_H

#include <stddef.h>

#define EOF (-1)

int putchar(int c);
int puts(const char *s);
int printf(const char *fmt, ...);

#endif
