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
