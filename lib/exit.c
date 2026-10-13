/*
 * exit, for a program that takes its address.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * A call to exit by name never comes here: acc writes the four instructions
 * that go back to the stub that called main at the call itself (see
 * exit_builtin in gen.c). There is nothing in C to write those with, which
 * is why exit was never in the library -- and why `void (*f)(int) = exit`
 * did not link, with nothing there for the pointer to point at.
 *
 * The body below is that same call, so acc compiles it into those same four
 * instructions, and this member is the function a pointer to exit reaches.
 * A program that only calls exit never refers to the name, so the link
 * leaves it out, as it does every member nothing asks for. gcc's pr54937
 * takes exit's address this way.
 *
 * _Exit is here too, since it is exit with the exit hook taken away -- all
 * but the release below.
 */
#include <stdlib.h>

void exit(int status)
{
    exit(status);
}

/* What has to be given back to MOS however the program ends, or NULL: a
 * key handler kbuf_init gave MOS (keyboard.c), which MOS would otherwise go
 * on calling into whatever is loaded where the program was. The exit hook
 * runs it after the files are closed (atexit.c), and _Exit runs it too. */
void (*__acc_release)(void);

/* The end without the exit hook: no atexit functions and no files closed,
 * which is what C99 7.20.4.4 says _Exit skips. The hook is pointed at a
 * function that does only the release, and then this is exit. */
extern void (*__acc_exit_hook)(void);

static void released(void)
{
    if (__acc_release)
        __acc_release();
}

void _Exit(int status)
{
    __acc_exit_hook = released;
    exit(status);
}
