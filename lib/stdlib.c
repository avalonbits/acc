/*
 * <stdlib.h>: the heap, and reading a number out of a string.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdlib.h>
#include <string.h>

/* Where the memory nobody owns begins and ends. The link fills these in: the
 * first is past everything the program has, which it only knows once it has
 * laid the program out, and the second is below where the stack will be. */
extern char acc_heap_start[];
extern char acc_heap_end[];

/* A block, and the whole of what this knows about the heap.
 *
 * Blocks in one list in address order, each saying how long it is and
 * whether it is in use. Allocating walks until something big enough turns
 * up; freeing marks it and joins it to the block after it if that is free
 * too. First fit and join-forwards is the least that is honest: it does not
 * lose memory to a pattern of allocation, and it is thirty lines rather than
 * three hundred.
 *
 * What it is not is fast. A walk is the length of the list, so a program
 * that holds thousands of small blocks pays for it. That is the trade for
 * now, and where it stops being the right one is when something here is
 * measured rather than supposed. */
typedef struct block {
    struct block *next;
    size_t        size;         /* the bytes after the header */
    int           used;
} block;

static block *heap;

static void heap_start(void)
{
    heap = (block *) acc_heap_start;
    heap->next = NULL;
    heap->size = (size_t) (acc_heap_end - acc_heap_start) - sizeof(block);
    heap->used = 0;
}

/* What is left of a block after `n` bytes, as a block of its own -- if there
 * is enough left for one to be worth having. */
static void split(block *b, size_t n)
{
    block *rest;

    if (b->size < n + sizeof(block) + 4)
        return;
    rest = (block *) ((char *) (b + 1) + n);
    rest->next = b->next;
    rest->size = b->size - n - sizeof(block);
    rest->used = 0;
    b->next = rest;
    b->size = n;
}

void *malloc(size_t n)
{
    block *b;

    if (!n)
        return NULL;
    if (!heap)
        heap_start();

    /* Rounded up to three bytes, which is what this machine's words are, so
     * that what comes back is as aligned as anything here needs. */
    n = (n + 2) & ~(size_t) 2;

    for (b = heap; b; b = b->next) {
        if (b->used || b->size < n)
            continue;
        split(b, n);
        b->used = 1;

        return b + 1;
    }

    return NULL;
}

void free(void *p)
{
    block *b, *scan;

    if (!p)
        return;
    b = (block *) p - 1;
    b->used = 0;

    /* Joined to what follows it, as far as that goes. Walking from the front
     * each time is what makes joining backwards unnecessary: a block freed
     * before this one will swallow it when its own turn comes. */
    for (scan = heap; scan; scan = scan->next) {
        while (!scan->used && scan->next && !scan->next->used) {
            scan->size += scan->next->size + sizeof(block);
            scan->next = scan->next->next;
        }
    }
}

void *calloc(size_t count, size_t size)
{
    size_t n = count * size;
    void *p = malloc(n);

    if (p)
        memset(p, 0, n);

    return p;
}

void *realloc(void *p, size_t n)
{
    block *b;
    void *fresh;

    if (!p)
        return malloc(n);
    if (!n) {
        free(p);

        return NULL;
    }
    b = (block *) p - 1;
    if (b->size >= n)
        return p;
    fresh = malloc(n);
    if (fresh)
        memcpy(fresh, p, b->size);
    free(p);

    return fresh;
}

int abs(int n)
{
    return n < 0 ? -n : n;
}

long labs(long n)
{
    return n < 0 ? -n : n;
}

/* Leading spaces, a sign, then digits: what C says atoi reads, and no more.
 * What follows the digits is not this function's business and there is no
 * way for it to say so. */
long atol(const char *s)
{
    long value = 0;
    int negative = 0;

    while (*s == 32 || (*s >= 9 && *s <= 13))
        s++;
    if (*s == '-' || *s == '+')
        negative = *s++ == '-';
    while (*s >= '0' && *s <= '9')
        value = value * 10 + (*s++ - '0');

    return negative ? -value : value;
}

int atoi(const char *s)
{
    return (int) atol(s);
}
