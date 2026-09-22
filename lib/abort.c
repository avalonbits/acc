/*
 * abort: the program stops, and says it went wrong.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * On its own and not beside the rest of <stdlib.h>, for the reason strdup
 * is on its own: a library member is taken or left whole, and the rest of
 * that file is the heap. A program that calls abort and never allocates
 * should not be given an allocator.
 *
 * C says abort ends the program by an implementation-defined form of
 * unsuccessful termination, and that it does not return. What it does here
 * is exit with 134, which is what a shell reports for a program a SIGABRT
 * ended -- 128 and the signal's six. Nothing on the Agon has signals; the
 * number is chosen because it is the one a person reading an exit status
 * already knows, and because it is far from the small numbers a test that
 * failed its own way comes back with.
 *
 * atexit is not run and no stream is flushed, which is what C asks for and
 * what exit here does anyway: acc has neither.
 */
#include <stdlib.h>

void abort(void)
{
    exit(134);
}
