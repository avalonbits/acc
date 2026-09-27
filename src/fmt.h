/*
 * acc -- a C compiler for the Agon Light.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * printf for the formats acc uses: see fmt.c.
 */
#ifndef ACC_FMT_H
#define ACC_FMT_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

int fmt_vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap);
int fmt_vfprintf(FILE *file, const char *fmt, va_list ap);

#endif
