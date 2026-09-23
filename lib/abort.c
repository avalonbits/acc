/*
 * abort: the program stops, and says it went wrong.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * In a file of its own, for the reason strdup is: while a link took a
 * library member whole, the rest of <stdlib.h>'s file was the heap, and a
 * program that calls abort and never allocates should not be given an
 * allocator. A link takes functions now, and that no longer depends on it.
 *
 * C says abort raises SIGABRT, and ends the program unless a handler for it
 * does not return. A handler can only have been set by signal(), so abort
 * reaches raise through a pointer signal() fills in, in signal()'s own
 * data, which abort names weakly: a program that never calls signal() has
 * no such pointer, the weak reference is null, and abort costs nothing but
 * itself. When there is one, raise does the rest -- calls the handler, or
 * ignores the signal, or ends the program as SIGABRT's default -- and if it
 * comes back, the program ends anyway.
 *
 * It ends with 134, which is what a shell reports for a program a SIGABRT
 * ended -- 128 and the signal's six, and what raise does for any signal
 * with no handler. atexit is not run and no stream is flushed, which is
 * what C asks for.
 */
#include <signal.h>
#include <stdlib.h>

#pragma weak __acc_raise
extern int (*__acc_raise)(int);

/* Through a pointer, since the address of a variable is never null as far
 * as C is concerned, and a compiler may say so without looking. */
static int (**raise_at)(int) = &__acc_raise;

void abort(void)
{
    if (raise_at)
        if (*raise_at)
            (*raise_at)(SIGABRT);
    exit(134);
}
