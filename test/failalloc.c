/*
 * A failure after the output is open: linked in with
 * -Wl,--wrap=fopen,--wrap=malloc,--wrap=calloc,--wrap=realloc, and once
 * the program has opened a file to write ("wb"), the next allocation, or
 * the next file it opens to read, fails. That is where a compile ran out of
 * memory on the Agon and left an object with nothing in it. See
 * test/abandon.sh.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *__real_fopen(const char *path, const char *mode);
void *__real_malloc(size_t n);
void *__real_calloc(size_t n, size_t size);
void *__real_realloc(void *p, size_t n);

static int armed;

FILE *__wrap_fopen(const char *path, const char *mode)
{
    if (armed && mode[0] == 'r') {
        armed = 0;

        return NULL;
    }
    if (!strcmp(mode, "wb"))
        armed = 1;

    return __real_fopen(path, mode);
}

void *__wrap_malloc(size_t n)
{
    if (armed) {
        armed = 0;

        return NULL;
    }

    return __real_malloc(n);
}

void *__wrap_calloc(size_t n, size_t size)
{
    if (armed) {
        armed = 0;

        return NULL;
    }

    return __real_calloc(n, size);
}

void *__wrap_realloc(void *p, size_t n)
{
    if (armed) {
        armed = 0;

        return NULL;
    }

    return __real_realloc(p, n);
}
