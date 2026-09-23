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
 */
#include <stdlib.h>

void exit(int status)
{
    exit(status);
}
