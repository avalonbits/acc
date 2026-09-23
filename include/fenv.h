/*
 * fenv.h -- the floating-point environment (C99 7.6).
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * There is not much of one. The arithmetic is a library of subroutines,
 * not a unit with status flags, and it rounds to nearest and does nothing
 * else. So this header says what C99 lets it say: no exception macro is
 * defined, which is how an implementation says it supports none of them,
 * and FE_TONEAREST is the one rounding direction. FE_ALL_EXCEPT is 0.
 *
 * The functions are all there and all succeed at what can be done: clearing,
 * testing or raising no exceptions, and setting the rounding mode to the one
 * it is. Asking for another mode fails, as C says it must.
 */
#ifndef ACC_FENV_H
#define ACC_FENV_H

typedef struct { char unused; } fenv_t;
typedef char fexcept_t;

#define FE_ALL_EXCEPT   0
#define FE_TONEAREST    0

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
