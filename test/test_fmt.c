/*
 * src/fmt.c's printf against the C library's, over every form acc uses and
 * the values at their edges: they have to agree byte for byte, the length
 * returned included, and a buffer too small has to be cut and terminated
 * as snprintf's is.
 */
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "fmt.h"

static int failures, checks;

static int ours(char *buf, size_t cap, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = fmt_vsnprintf(buf, cap, fmt, ap);
    va_end(ap);

    return n;
}

#define SAME(cap, fmt, ...) do {                                            \
        char a[64], b[64];                                                  \
        int na, nb;                                                         \
                                                                            \
        memset(a, 'x', sizeof a);                                           \
        memset(b, 'x', sizeof b);                                           \
        na = ours(a, cap, fmt, __VA_ARGS__);                                \
        nb = snprintf(b, cap, fmt, __VA_ARGS__);                            \
        checks++;                                                           \
        if (na != nb || memcmp(a, b, sizeof a)) {                           \
            fprintf(stderr, "  FAIL %-12s got \"%s\" (%d), want \"%s\" (%d)\n", \
                    fmt, a, na, b, nb);                                     \
            failures++;                                                     \
        }                                                                   \
    } while (0)

int main(void)
{
    int ints[] = { 0, 1, -1, 7, 42, -42, 999, 100000, -8388608, 8388607,
                   INT_MIN, INT_MAX };
    unsigned us[] = { 0, 1, 9, 10, 255, 4096, 0xabcdef, UINT_MAX };
    long ls[] = { 0L, -1L, 123456789L, -2147483647L - 1, 2147483647L };
    unsigned long uls[] = { 0UL, 1UL, 4294967295UL, 305419896UL };
    size_t i;

    for (i = 0; i < sizeof ints / sizeof *ints; i++) {
        SAME(64, "%d", ints[i]);
        SAME(64, "[%5d]", ints[i]);
        SAME(64, "[%05d]", ints[i]);
    }
    for (i = 0; i < sizeof us / sizeof *us; i++) {
        SAME(64, "%u", us[i]);
        SAME(64, "%02u", us[i]);
        SAME(64, "%x", us[i]);
        SAME(64, "%06x", us[i]);
    }
    for (i = 0; i < sizeof ls / sizeof *ls; i++)
        SAME(64, "%ld", ls[i]);
    for (i = 0; i < sizeof uls / sizeof *uls; i++) {
        SAME(64, "%lu", uls[i]);
        SAME(64, "%lx", uls[i]);
    }
    SAME(64, "%c%c%c", 'a', 'Z', '0');
    SAME(64, "%s and %s", "one", "");
    SAME(64, "[%8s]", "pad");
    SAME(64, "[%.*s]", 3, "abcdef");
    SAME(64, "[%.*s]", 0, "abcdef");
    SAME(64, "[%.*s]", 10, "abc");
    SAME(64, "%.*s is not a character", 6, "\\u0041");
    SAME(64, "100%% of %s", "it");
    SAME(64, "%s:%d:%d: error: %s", "file.c", 12, 7, "text");
    SAME(64, "Done in %u.%02u seconds\r\n", 3u, 5u);
    /* Buffers too small, cut as snprintf cuts them; the sizes are
     * variables so the compiler does not warn that they are too small. */
    for (i = 0; i < 3; i++) {
        size_t caps[] = { 8, 1, 0 };

        SAME(caps[i], "%s", "longer than the buffer");
        SAME(caps[i], "%d", 12345);
    }

    fprintf(stderr, "  fmt: %d checks, %d failed\n", checks, failures);

    return failures != 0;
}
