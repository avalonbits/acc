/*
 * A failure after the output is open: linked in with
 * -Wl,--wrap=fopen,--wrap=fwrite,--wrap=malloc,--wrap=calloc,--wrap=realloc,
 * and once the program has opened a file to write ("wb"), the next
 * allocation, the next file it opens to read, or the next write -- a card
 * that is full -- fails. That is where a compile ran out of
 * memory on the Agon and left an object with nothing in it. See
 * test/abandon.sh.
 *
 * With FAILALLOC_OVER set to a number, what fails instead is every
 * allocation of more than that many bytes once the file is open, and
 * nothing else: what test/objmem.sh holds the object writer to. And with
 * FAILALLOC_FROM_START as well, from the start: test/linkfixups.sh.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *__real_fopen(const char *path, const char *mode);
size_t __real_fwrite(const void *p, size_t size, size_t n, FILE *f);
void *__real_malloc(size_t n);
void *__real_calloc(size_t n, size_t size);
void *__real_realloc(void *p, size_t n);

static int armed;
static size_t over;             /* FAILALLOC_OVER, or 0 */

__attribute__((constructor))
static void from_start(void)
{
    const char *limit = getenv("FAILALLOC_OVER");

    if (getenv("FAILALLOC_FROM_START") && limit) {
        over = (size_t) atol(limit);
        armed = 1;
    }
}

/* Whether an allocation of `n` fails, now. */
static int fails(size_t n)
{
    if (!armed)
        return 0;
    /* A reserve of 16 MB and more is the host's own -- the names' arena --
     * and not something the Agon's build asks for at all. */
    if (over)
        return n > over && n < ((size_t) 16 << 20);
    armed = 0;

    return 1;
}

FILE *__wrap_fopen(const char *path, const char *mode)
{
    if (armed && !over && mode[0] == 'r') {
        armed = 0;

        return NULL;
    }
    if (!strcmp(mode, "wb")) {
        const char *limit = getenv("FAILALLOC_OVER");

        over = limit ? (size_t) atol(limit) : 0;
        armed = 1;
    }

    return __real_fopen(path, mode);
}

size_t __wrap_fwrite(const void *p, size_t size, size_t n, FILE *f)
{
    if (armed && !over) {
        armed = 0;

        return 0;
    }

    return __real_fwrite(p, size, n, f);
}

void *__wrap_malloc(size_t n)
{
    if (fails(n))
        return NULL;

    return __real_malloc(n);
}

void *__wrap_calloc(size_t n, size_t size)
{
    if (fails(n * size))
        return NULL;

    return __real_calloc(n, size);
}

void *__wrap_realloc(void *p, size_t n)
{
    if (fails(n))
        return NULL;

    return __real_realloc(p, n);
}
