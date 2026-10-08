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
 * it is in use, and where the one in front of it starts -- so that freeing
 * joins a block to whichever of its two neighbours is free with two tests
 * and no walk. The free ones are in a second list as well, threaded
 * through their own bytes, and allocating walks only that: first fit among
 * the free blocks, not among all of them. Walking all of them made a
 * program that holds many blocks pay for every one at every malloc: Busybox
 * vi, substituting through 40 KB with an undo record a change, spent 70% of
 * its time in malloc's walk. A block freed goes to the front of the free
 * list; what is left of one split takes its place there, so the room at the
 * end of the heap stays where it was.
 *
 * realloc grows a block into a free one after it, rather than copying it
 * elsewhere, where that is room enough: a buffer grown a little at a time
 * was copied whole each time. */
typedef struct block {
    struct block *next;
    struct block *prev;
    size_t        size;         /* the bytes after the header */
    unsigned char used;
} block;

/* A free block's first bytes: its neighbours in the free list. */
typedef struct {
    block *next;
    block *prev;
} links;

#define LINKS(b) ((links *) ((b) + 1))

/* malloc is lib/malloc.s, which reads these and calls the two below: their
 * names are the library's, not a program's. It takes the block's and the
 * links' layout as they are here. */
#define heap        acc_heap
#define free_list   acc_free_list
#define heap_start  acc_heap_begin
#define free_take   acc_free_take

typedef char block_is_ten_bytes[sizeof(block) == 10 ? 1 : -1];
typedef char links_are_six_bytes[sizeof(links) == 6 ? 1 : -1];

block *heap, *free_list;

/* `b` into the free list, in front, or in `at`'s place. */
static void free_put(block *b, block *before, block *after)
{
    LINKS(b)->prev = before;
    LINKS(b)->next = after;
    if (before)
        LINKS(before)->next = b;
    else
        free_list = b;
    if (after)
        LINKS(after)->prev = b;
}

void free_take(block *b)
{
    block *before = LINKS(b)->prev, *after = LINKS(b)->next;

    if (before)
        LINKS(before)->next = after;
    else
        free_list = after;
    if (after)
        LINKS(after)->prev = before;
}

void heap_start(void)
{
    heap = (block *) acc_heap_start;
    heap->next = NULL;
    heap->prev = NULL;
    heap->size = (size_t) (acc_heap_end - acc_heap_start) - sizeof(block);
    heap->used = 0;
    free_list = NULL;
    free_put(heap, NULL, NULL);
}

/* What is left of a block after `n` bytes, as a free block of its own -- if
 * there is enough left for one to be worth having -- in the free list
 * where the block was, or in front where it was not free. */
static void split(block *b, size_t n, int was_free)
{
    block *rest;

    if (b->size < n + sizeof(block) + sizeof(links) + 4) {
        if (was_free)
            free_take(b);
        return;
    }
    rest = (block *) ((char *) (b + 1) + n);
    rest->next = b->next;
    rest->prev = b;
    rest->size = b->size - n - sizeof(block);
    rest->used = 0;
    if (rest->next)
        rest->next->prev = rest;
    b->next = rest;
    b->size = n;
    if (was_free)
        free_put(rest, LINKS(b)->prev, LINKS(b)->next);
    else
        free_put(rest, NULL, free_list);
}

/* Rounded up to three bytes, which is what this machine's words are, so
 * that what comes back is as aligned as anything here needs -- and to room
 * for the links once it is free again. */
static size_t rounded(size_t n)
{
    n = (n + 2) & ~(size_t) 2;

    return n < sizeof(links) ? sizeof(links) : n;
}

/* A block and the free block after it, made one: the second out of the
 * free list, the first where it was. */
static void join(block *b)
{
    block *rest = b->next;

    free_take(rest);
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
     * a program gives back is room it can ask for again as one piece. */
    if (b->next && !b->next->used)
        join(b);
    if (b->prev && !b->prev->used) {
        block *front = b->prev;         /* free, and in the list already */

        front->size += b->size + sizeof(block);
        front->next = b->next;
        if (b->next)
            b->next->prev = front;

        return;
    }
    free_put(b, NULL, free_list);
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
    n = rounded(n);
    if (b->next && !b->next->used && b->size + sizeof(block) + b->next->size >= n) {
        join(b);
        split(b, n, 0);

        return p;
    }
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
