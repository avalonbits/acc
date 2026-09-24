/*
 * signal and raise, for a machine where no signal comes from outside.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <signal.h>
#include <stdlib.h>

/* What each signal is set to, by its number; SIG_DFL, which is 0, until a
 * program says otherwise. */
static void (*handlers[16])(int);

/* raise, for abort, which names this weakly -- see abort.c. Set by signal()
 * rather than initialised, since a variable with no initial value is in the
 * data a link takes along with any function of this file, and a function
 * that nothing calls by name is not taken at all. */
int (*__acc_raise)(int);

void (*signal(int sig, void (*func)(int)))(int)
{
    void (*old)(int);

    if (sig <= 0 || sig > 15)
        return SIG_ERR;
    old = handlers[sig];
    handlers[sig] = func;
    __acc_raise = raise;

    return old;
}

/* The handler, if there is one, with the signal put back to its default
 * first -- the first of the two things C allows, and the one that keeps a
 * handler that raises its own signal from calling itself for ever. With no
 * handler the program ends, as though a signal had ended it. */
int raise(int sig)
{
    void (*handler)(int);

    if (sig <= 0 || sig > 15)
        return -1;
    handler = handlers[sig];
    if (handler == SIG_IGN)
        return 0;
    if (handler == SIG_DFL)
        _Exit(128 + sig);
    handlers[sig] = SIG_DFL;
    handler(sig);

    return 0;
}
