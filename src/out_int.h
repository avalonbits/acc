/*
 * What image.c and reloc.c share: the file the image goes to, and the
 * runs a cut takes out of it. What the rest of acc sees of them is in
 * acc.h.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_OUT_INT_H
#define ACC_OUT_INT_H

#include <stdio.h>

#include "acc.h"

#ifdef ACC_TABLE_STATS
extern int out_nflushes, out_nspill_cuts, out_nresidents;
#define COUNT(n) ((n)++)
#else
#define COUNT(n) ((void) 0)
#endif

#define OUT_LEN ((int) (out_put - out_img))

/* image.c */
unsigned char *piece(int *room);
void spill_io(int off, unsigned char *buf, int n, int write);
void slots_done(void);

#endif
