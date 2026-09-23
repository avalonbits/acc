/*
 * <fenv.h>, for arithmetic with no exceptions and one rounding mode.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Each function does what C asks of it given that the set of exceptions is
 * empty: there is never a flag to clear, save, raise or find, so each
 * succeeds. The rounding mode is always to nearest, and a request for any
 * other is the one thing here that fails.
 */
#include <fenv.h>

const fenv_t __acc_fe_dfl_env;

int feclearexcept(int excepts)
{
    return excepts != 0;
}

int fegetexceptflag(fexcept_t *flagp, int excepts)
{
    *flagp = 0;

    return excepts != 0;
}

int feraiseexcept(int excepts)
{
    return excepts != 0;
}

int fesetexceptflag(const fexcept_t *flagp, int excepts)
{
    return excepts != 0;
}

int fetestexcept(int excepts)
{
    return 0;
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
    envp->unused = 0;

    return 0;
}

int feholdexcept(fenv_t *envp)
{
    envp->unused = 0;

    return 0;
}

int fesetenv(const fenv_t *envp)
{
    return 0;
}

int feupdateenv(const fenv_t *envp)
{
    return 0;
}
