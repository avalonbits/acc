/*
 * Names: every identifier, interned once. See names.c.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_NAMES_H
#define ACC_NAMES_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "types.h"

/* NameRef is declared with Type, at the top: the struct members below the
 * types are named by one. */

NameRef     name_intern(const char *text, int len);
const char *name_text(NameRef ref);
int         name_weak(NameRef ref);             /* #pragma weak said so */
void        name_set_weak(NameRef ref, int weak);
int         name_missed(NameRef ref);           /* a library has it not */
void        name_set_missed(NameRef ref, int missed);
void        name_init(void);

extern char *name_arena;    /* for name_global, further down */

/* The floating literal at s, as the bits of the nearest float; returns where
 * it ends, which is s if it has no digits. In float.c. */
const char *float_literal(const char *s, uint32_t *bits);
uint32_t    float_from_int(uint64_t magnitude, int negative);

/* Folding a constant expression, in integers rather than in the float of
 * whichever compiler built this one: see the head of the arithmetic in
 * float.c for why the host's own is not good enough. */
uint32_t    float_add(uint32_t a, uint32_t b);
uint32_t    float_mul(uint32_t a, uint32_t b);
uint32_t    float_div(uint32_t a, uint32_t b);
uint32_t    float_neg(uint32_t a);
int64_t     float_to_int(uint32_t a);
int         float_is_nan(uint32_t a);
int         float_is_inf(uint32_t a);
int         float_is_zero(uint32_t a);
int         float_compare(uint32_t a, uint32_t b);  /* -1, 0, 1; 2 unordered */

#endif
