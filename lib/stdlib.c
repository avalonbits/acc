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
 * Blocks in one list in address order, each saying how long it is, whether
 * it is in use, and where the one in front of it starts. Allocating walks
 * until something big enough turns up; freeing marks the block and joins it
 * to whichever of its two neighbours is free.
 *
 * The link backwards is what makes the second half of that cheap. Without
 * it, joining to the block in front means finding it, and finding it means
 * walking from the start of the heap -- so every free costs the length of
 * the list, and a program that frees as it goes pays for the whole heap
 * every time it lets go of anything.
 *
 * That is insurance and not a speed-up: it is one byte a block, and on the
 * one program it has been measured against -- zap, assembling a 6 KB source
 * twelve times -- the walk and the two tests came to the same time to the
 * hundredth. What it buys is that there is no length of list at which
 * letting go of something becomes the expensive part.
 *
 * First fit, and no more than that. It does not lose memory to a pattern of
 * allocation, and it is forty lines rather than three hundred. What it is
 * still not is fast to allocate from: the walk is the length of the list,
 * so a program holding thousands of small blocks pays for it at every
 * malloc. Where that stops being the right trade is when something here is
 * measured rather than supposed. */
typedef struct block {
    struct block *next;
    struct block *prev;
    size_t        size;         /* the bytes after the header */
    unsigned char used;
} block;

static block *heap;

static void heap_start(void)
{
    heap = (block *) acc_heap_start;
    heap->next = NULL;
    heap->prev = NULL;
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
    rest->prev = b;
    rest->size = b->size - n - sizeof(block);
    rest->used = 0;
    if (rest->next)
        rest->next->prev = rest;
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

/* A free block and the free block after it, made one. */
static void join(block *b)
{
    block *rest = b->next;

    b->size += rest->size + sizeof(block);
    b->next = rest->next;
    if (rest->next)
        rest->next->prev = b;
}

void free(void *p)
{
    block *b;

    if (!p)
        return;
    b = (block *) p - 1;
    b->used = 0;

    /* Joined to the block after it and to the one in front, so that the room
     * a program gives back is room it can ask for again as one piece. Two
     * tests and no walk: that is what the link backwards is for. */
    if (b->next && !b->next->used)
        join(b);
    if (b->prev && !b->prev->used)
        join(b->prev);
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

long long llabs(long long n)
{
    return n < 0 ? -n : n;
}

/* The quotient and the remainder of one division. C99's division truncates
 * toward zero, which is what the helpers do, so this is the two operators
 * and nothing more. */
div_t div(int a, int b)
{
    div_t r;

    r.quot = a / b;
    r.rem = a % b;

    return r;
}

ldiv_t ldiv(long a, long b)
{
    ldiv_t r;

    r.quot = a / b;
    r.rem = a % b;

    return r;
}

lldiv_t lldiv(long long a, long long b)
{
    lldiv_t r;

    r.quot = a / b;
    r.rem = a % b;

    return r;
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

long long atoll(const char *s)
{
    long long value = 0;
    int negative = 0;

    while (*s == 32 || (*s >= 9 && *s <= 13))
        s++;
    if (*s == '-' || *s == '+')
        negative = *s++ == '-';
    while (*s >= '0' && *s <= '9')
        value = value * 10 + (*s++ - '0');

    return negative ? -value : value;
}
