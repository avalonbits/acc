/*
 * setjmp.h -- nonlocal jumps (C99 7.13).
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Both are in acc's runtime rather than the library, since they are the
 * stack pointer and the frame pointer and C cannot say either: see the
 * setjmp section of src/rt/helpers.s. A jmp_buf is where setjmp was called
 * from, IX, the stack pointer and IY.
 */
#pragma once
#ifndef ACC_SETJMP_H
#define ACC_SETJMP_H

typedef int jmp_buf[4];

int  setjmp(jmp_buf env);
__attribute__((noreturn)) void longjmp(jmp_buf env, int val);

#endif
