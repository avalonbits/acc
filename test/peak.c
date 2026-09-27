/*
 * The most heap a program held at once, printed as it exits: linked in with
 * -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free, which sends
 * its allocations through here. A realloc is a malloc, a copy and a free,
 * which is what agondev's is, so that a growth counts both blocks as it
 * does on the Agon. See test/linkheap.sh.
 */
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *__real_malloc(size_t n);
void *__real_calloc(size_t n, size_t size);
void  __real_free(void *p);

static size_t live, peak;

static void *held(void *p)
{
    if (p) {
        live += malloc_usable_size(p);
        if (live > peak)
            peak = live;
    }

    return p;
}

void *__wrap_malloc(size_t n)
{
    return held(__real_malloc(n));
}

void *__wrap_calloc(size_t n, size_t size)
{
    return held(__real_calloc(n, size));
}

void __wrap_free(void *p)
{
    if (p)
        live -= malloc_usable_size(p);
    __real_free(p);
}

void *__wrap_realloc(void *p, size_t n)
{
    void *q = held(__real_malloc(n));
    size_t was = p ? malloc_usable_size(p) : 0;

    if (q && p) {
        memcpy(q, p, was < n ? was : n);
        __wrap_free(p);
    }

    return q;
}

__attribute__((destructor))
static void report(void)
{
    fprintf(stderr, "peak %zu\n", peak);
}
