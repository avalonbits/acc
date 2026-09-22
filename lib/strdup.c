/*
 * strdup: a string copied into room from the heap.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * On its own and not beside the rest of <string.h>, because it is the only
 * one of them that wants the heap. A library member is taken or left whole,
 * so putting it with strlen would give the heap to every program that
 * measures a string -- which is nearly all of them, and none of which asked.
 */
#include <stdlib.h>
#include <string.h>

char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *to = malloc(n);

    if (to)
        memcpy(to, s, n);

    return to;
}
