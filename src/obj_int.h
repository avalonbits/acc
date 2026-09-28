/*
 * What obj.c and archive.c share: writing the numbers the two formats are
 * made of, and the file being written. What the rest of acc sees of them
 * is in acc.h.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_OBJ_INT_H
#define ACC_OBJ_INT_H

#include <stdio.h>

#include "acc.h"
#include "acc_build.h"

/* Zero when the build could not work out what it is -- see src/build_id.sh.
 * A compiler that cannot say which one it is cannot vouch for an object it
 * finds, so it makes every one of them again. */
#ifndef ACC_BUILD
#define ACC_BUILD 0
#endif

#define OBJ_HEADER   31

/* obj.c */
extern char *strings;
extern int strings_len, strings_cap;
int string_add(const char *text);
void put_num(FILE *f, int value);
FILE *write_open(const char *path);
void write_done(void);

#endif
