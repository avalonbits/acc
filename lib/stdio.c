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

/* Whether the last character to the screen was a return. */
static unsigned char after_return;

/* A character to the screen, for everything in the library that writes
 * text there: a newline is a return and a line feed, which is what MOS's
 * console wants and what agondev's putchar sends -- but only the line feed
 * when the program has just sent the return itself, so that its own
 * "\r\n" is not "\r\r\n". putch sends the byte as it is, and goes round
 * this: a 13 among a VDU command's numbers is no return on the screen. */
int __acc_console_putc(int c)
{
    unsigned char byte = (unsigned char) c;

    if (byte == '\n' && !after_return)
        acc_rt_putch('\r');
    after_return = byte == '\r';

    return acc_rt_putch(byte);
}

static int out(int c)
{
    return __acc_stdout_putc ? __acc_stdout_putc(c) : __acc_console_putc(c);
}

int putchar(int c)
{
    return out(c);
}

/* And a newline after it, as C says: each a return and a line feed on the
 * screen, as out sends every newline there. */
int puts(const char *s)
{
    while (*s)
        out(*s++);
    out('\n');

    return 0;
}
