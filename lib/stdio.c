/*
 * putchar and puts, which write to the console -- or to wherever freopen
 * has sent stdout.
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

/* Where stdout goes when freopen has made it a file: set by file.c, and
 * null while stdout is the screen, which is written to directly so that a
 * program that prints does not carry the file layer. printf reads it too. */
int (*__acc_stdout_putc)(int c);

static int out(int c)
{
    return __acc_stdout_putc ? __acc_stdout_putc(c) : acc_rt_putch(c);
}

int putchar(int c)
{
    return out(c);
}

/* And a newline after it, as C says -- which on this machine is a return and
 * a line feed, since that is what MOS's console expects. */
int puts(const char *s)
{
    while (*s)
        out(*s++);
    if (!__acc_stdout_putc)
        acc_rt_putch('\r');
    out('\n');

    return 0;
}
