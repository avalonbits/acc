/*
 * <fenv.h>: the exception flags kept here, and one rounding mode.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * No arithmetic raises an exception -- it is a library of subroutines with
 * no status flags, as agondev's is -- so the flags are what the program
 * set with feraiseexcept and fesetexceptflag, kept, cleared, saved and put
 * back as C99 7.6 says. A bit outside FE_ALL_EXCEPT is no exception, and
 * a function asked for one answers nonzero, having done what it could with
 * the rest. The rounding mode is always to nearest, and a request for any
 * other fails.
 */
#include <fenv.h>

const fenv_t __acc_fe_dfl_env;

static unsigned char raised;            /* the flags set, of FE_ALL_EXCEPT */

int feclearexcept(int excepts)
{
    raised &= (unsigned char) ~excepts;

    return (excepts & ~FE_ALL_EXCEPT) != 0;
}

int fegetexceptflag(fexcept_t *flagp, int excepts)
{
    *flagp = (fexcept_t) (raised & excepts);

    return (excepts & ~FE_ALL_EXCEPT) != 0;
}

int feraiseexcept(int excepts)
{
    raised |= (unsigned char) (excepts & FE_ALL_EXCEPT);

    return (excepts & ~FE_ALL_EXCEPT) != 0;
}

int fesetexceptflag(const fexcept_t *flagp, int excepts)
{
    excepts &= FE_ALL_EXCEPT;
    raised = (unsigned char) ((raised & ~excepts) | (*flagp & excepts));

    return 0;
}

int fetestexcept(int excepts)
{
    return raised & excepts & FE_ALL_EXCEPT;
}

int fegetround(void)
{
    return FE_TONEAREST;
}

int fesetround(int round)
{
    return round != FE_TONEAREST;
}

int fegetenv(fenv_t *envp)
{
    envp->flags = raised;

    return 0;
}

int feholdexcept(fenv_t *envp)
{
    envp->flags = raised;
    raised = 0;

    return 0;
}

int fesetenv(const fenv_t *envp)
{
    raised = envp->flags;

    return 0;
}

/* The environment put back, and what was raised since kept raised. */
int feupdateenv(const fenv_t *envp)
{
    raised |= envp->flags;

    return 0;
}
