/*
 * What <stdio.h> has so far: the two that write to the console.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

/* In acc's own runtime, which is where the instruction that asks MOS to
 * print a character lives: acc has no way to write one in C, and a library
 * that had to be assembled would be a library the Agon could not build for
 * itself. Declared rather than included, because it is acc's and not the
 * program's. */
int acc_rt_putch(int c);

int putchar(int c)
{
    return acc_rt_putch(c);
}

/* And a newline after it, as C says -- which on this machine is a return and
 * a line feed, since that is what MOS's console expects. */
int puts(const char *s)
{
    while (*s)
        acc_rt_putch(*s++);
    acc_rt_putch('\r');
    acc_rt_putch('\n');

    return 0;
}
