/*
 * atexit, and what runs when the program ends.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The startup stub calls through a cell named __acc_exit_hook on the way
 * out, whether main returned or exit sent it back -- see src/rt/startup.s.
 * It starts pointing at a `ret`. atexit points it at at_exit below, which
 * runs the registered functions last first, as C99 7.20.4.3 says, and then
 * has the file layer flush and close every file still open. The file layer
 * arms it the same way when it opens one, so a program that writes a file
 * and never closes it still finds it complete on the card.
 *
 * A program that does neither calls a `ret` and takes none of this.
 */
#include <stdlib.h>

extern void (*__acc_exit_hook)(void);

/* The file layer's, set when it opens a file; see file.c. */
void (*__acc_close_files)(void);

#define ATEXIT_MAX 32           /* C99 asks for at least 32 */

static void (*handlers[ATEXIT_MAX])(void);
static int count;

static void at_exit(void)
{
    while (count > 0)
        handlers[--count]();
    if (__acc_close_files)
        __acc_close_files();
}

/* That something wants the end of the program to run at_exit. */
void __acc_exit_arm(void)
{
    __acc_exit_hook = at_exit;
}

int atexit(void (*f)(void))
{
    if (count == ATEXIT_MAX)
        return -1;
    handlers[count++] = f;
    __acc_exit_arm();

    return 0;
}
