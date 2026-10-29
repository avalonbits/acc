/*
 * fenv.h -- the floating-point environment (C99 7.6).
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * There is not much of one. The arithmetic is a library of subroutines,
 * not a unit with status flags, and it rounds to nearest and does nothing
 * else. The five exceptions are agondev's, at its values, so that a
 * program written for it builds: the flags are kept by the functions here
 * -- feraiseexcept sets them, fetestexcept finds them -- and no arithmetic
 * raises one, as none of agondev's does. The four rounding directions are
 * named, and FE_TONEAREST is the one fesetround takes: asked for another,
 * it fails, as C says it must, and the rounding stays to nearest.
 */
#pragma once
#ifndef ACC_FENV_H
#define ACC_FENV_H

typedef struct { unsigned char flags; } fenv_t;    /* the flags it holds */
typedef unsigned char fexcept_t;

#define FE_DIVBYZERO    (1 << 6)        /* agondev's values */
#define FE_INEXACT      (1 << 5)
#define FE_INVALID      (1 << 4)
#define FE_OVERFLOW     (1 << 3)
#define FE_UNDERFLOW    (1 << 2)
#define FE_ALL_EXCEPT   (FE_DIVBYZERO | FE_INEXACT | FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW)

#define FE_TONEAREST    0               /* the one fesetround takes */
#define FE_TOWARDZERO   1
#define FE_DOWNWARD     2
#define FE_UPWARD       3

extern const fenv_t __acc_fe_dfl_env;
#define FE_DFL_ENV      (&__acc_fe_dfl_env)

int feclearexcept(int excepts);
int fegetexceptflag(fexcept_t *flagp, int excepts);
int feraiseexcept(int excepts);
int fesetexceptflag(const fexcept_t *flagp, int excepts);
int fetestexcept(int excepts);

int fegetround(void);
int fesetround(int round);

int fegetenv(fenv_t *envp);
int feholdexcept(fenv_t *envp);
int fesetenv(const fenv_t *envp);
int feupdateenv(const fenv_t *envp);

#endif
