/*
 * qsort, in its own object so that a program that does not sort carries none
 * of it.
 *
 * A shell sort, and not the quicksort the name promises. Three reasons, all
 * of them about this machine. It needs no stack beyond its own frame, where
 * a quicksort needs a recursion or a stack of partitions, and the stack here
 * shares its region with the heap. It is thirty lines. And it has no worst
 * case worth fearing: the gaps are Knuth's, which put it at O(n^1.5) on the
 * inputs a program on an 18 MHz machine is going to hand it.
 *
 * C does not promise a stable sort, and this one is not.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdlib.h>

/* Two elements exchanged a byte at a time: the size is the caller's and
 * nothing here can know it is a width the chip can move in one go. */
static void swap_bytes(char *a, char *b, size_t size)
{
    while (size--) {
        char t = *a;

        *a++ = *b;
        *b++ = t;
    }
}

void qsort(void *base, size_t nmemb, size_t size,
           int (*cmp)(const void *, const void *))
{
    char  *items = base;
    size_t gap;

    if (nmemb < 2 || !size)
        return;

    /* 1, 4, 13, 40, ...: the largest of Knuth's gaps under a third of
     * the length, which is where he has it start. */
    for (gap = 1; gap < nmemb / 3; gap = gap * 3 + 1)
        ;

    for (; gap > 0; gap = (gap - 1) / 3) {
        size_t i;

        for (i = gap; i < nmemb; i++) {
            size_t j = i;

            while (j >= gap
                   && cmp(items + (j - gap) * size, items + j * size) > 0) {
                swap_bytes(items + (j - gap) * size, items + j * size, size);
                j -= gap;
            }
        }
    }
}
