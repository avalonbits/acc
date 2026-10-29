/* <stdlib.h> against libagon's: the conversions on edges -- signs, bases,
 * prefixes, overflow and where they stop -- div and abs, qsort and
 * bsearch, rand after srand (acc's is agondev's generator), and the heap's
 * answers to 0 and to sizes it cannot give.
 *
 * Where libagon departs from C99 acc does not follow, and those cases are
 * not here but in test/hosted/, held to glibc: strtol and strtoul take
 * "0x" with no hex digit after it as nothing (C: the 0); strtod takes ".",
 * "e5", "1e" and "1e+" as numbers, reads no hex float, inf or nan, and
 * answers 0 without ERANGE for 1e40; calloc of a size past 24 bits wraps
 * to a small block where acc answers NULL. */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmp(const void *a, const void *b)
{
    int x = *(const int *) a, y = *(const int *) b;

    return x < y ? -1 : x > y;
}

int main(void)
{
    static const char *ints[] = { "0", "42", "-42", "+7", "  12x", "\t\n-0", "0x1f", "0X1F", "017", "08",
                                  "2147483647", "2147483648", "-2147483648", "-2147483649", "99999999999",
                                  "", "+", "-", " ", "abc", "1e3", "  +0x10" };
    static const int bases[] = { 10, 0, 16, 8, 2, 36 };
    static const char *lls[] = { "0", "-1", "123456789012", "9223372036854775807", "9223372036854775808",
                                 "-9223372036854775808", "-9223372036854775809", "99999999999999999999" };
    static const char *dbls[] = { "0", "1.5", "-2.25", "  .5", "1e3", "1E-2", "3.", "1e-50", "123abc",
                                  "+.5e1x", "-0.75e+2x" };
    int a[12] = { 5, -3, 9, 0, 9, 2, -7, 4, 1, 8, -3, 6 };
    unsigned i, k;
    char *end;
    long v;
    unsigned long u;
    double d;
    void *p;

    for (i = 0; i < sizeof ints / sizeof *ints; i++) {
        printf("[%s]", ints[i]);
        for (k = 0; k < sizeof bases / sizeof *bases; k++) {
            errno = 0;
            v = strtol(ints[i], &end, bases[k]);
            printf(" %d:%ld,%d,%d", bases[k], v, (int) (end - ints[i]), errno == ERANGE);
        }
        errno = 0;
        u = strtoul(ints[i], &end, 0);
        printf(" u:%lu,%d,%d", u, (int) (end - ints[i]), errno == ERANGE);
        printf(" atoi:%d atol:%ld", atoi(ints[i]), atol(ints[i]));
        printf("\n");
    }
    for (i = 0; i < sizeof lls / sizeof *lls; i++) {
        long long ll = atoll(lls[i]);           /* libagon's printf has no %lld */

        printf("atoll [%s] %08lx%08lx\n", lls[i], (unsigned long) (ll >> 32), (unsigned long) ll);
    }
    for (i = 0; i < sizeof dbls / sizeof *dbls; i++) {
        errno = 0;
        d = strtod(dbls[i], &end);
        printf("[%s] %f,%d,%d atof %f\n", dbls[i], d, (int) (end - dbls[i]), errno == ERANGE, atof(dbls[i]));
    }
    printf("abs %d %ld div %d %d %d %d ldiv %ld %ld\n", abs(-5), labs(-70000L), div(7, -2).quot, div(7, -2).rem,
           div(-7, 2).quot, div(-7, 2).rem, ldiv(-100000L, 7).quot, ldiv(-100000L, 7).rem);
    qsort(a, 12, sizeof a[0], cmp);
    for (i = 0; i < 12; i++)
        printf("%d ", a[i]);
    k = 9;
    p = bsearch(&k, a, 12, sizeof a[0], cmp);
    printf("| 9 found %d", p ? *(int *) p : -1);   /* which of the two: C leaves it */
    k = 3;
    printf(" 3 at %d\n", bsearch(&k, a, 12, sizeof a[0], cmp) ? 1 : -1);
    printf("RAND_MAX %d\n", RAND_MAX);
    srand(1);
    for (i = 0; i < 8; i++)
        printf("%d ", rand());
    srand(12345);
    for (i = 0; i < 4; i++)
        printf("%d ", rand());
    printf("\n");
    p = malloc(0);
    printf("malloc0 %d", p != NULL);
    free(p);
    p = calloc(0, 4);
    printf(" calloc0 %d", p != NULL);
    free(p);
    p = realloc(malloc(4), 0);
    printf(" realloc0 %d", p != NULL);
    free(p);
    printf(" huge %d", malloc(0xFFFFFF) == NULL);
    p = malloc(8);
    memcpy(p, "1234567", 8);
    p = realloc(p, 200);
    printf(" realloc kept [%s]", (char *) p);
    printf(" realloc huge %d", realloc(p, 0xFFFFFF) == NULL);
    free(p);
    printf(" free null ok\n");
    free(NULL);
    return 0;
}
