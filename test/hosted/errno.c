/* <errno.h>, and the functions in <stdlib.h> and <string.h> that set it or
 * read it: the strto family, strerror, and div and its kin. */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *texts[] = {
    "0", "42", "  -17xyz", "+9", "0x1F", "0X", "0xg", "-0x10", "017", "08",
    "zz", "", "   ", "-", "+-3", "\t\n 12", "1010", "7fff", "Zz",
};
static const int bases[] = { 0, 2, 8, 10, 16, 36 };

static void one(int i, const char *s, int base)
{
    char *end;
    long l;
    unsigned long ul;
    long long ll;
    unsigned long long ull;

    errno = 0;
    l = strtol(s, &end, base);
    printf("strtol(texts[%d], %d) = %ld, end %d, errno %d\n",
           i, base, l, (int) (end - s), errno);
    ul = strtoul(s, &end, base);
    printf("  strtoul %lu (%d)", ul & 0xffffff, (int) (end - s));
    ll = strtoll(s, &end, base);
    printf("  strtoll %lld (%d)", ll, (int) (end - s));
    ull = strtoull(s, &end, base);
    printf("  strtoull %llu (%d)\n", ull & 0xffffff, (int) (end - s));
}

/* A number one past the largest of its type, whatever that type's width is
 * on the machine running this. */
static void past(void)
{
    char buf[40];
    char *end;
    long l;
    long long ll;

    sprintf(buf, "%lu", (unsigned long) LONG_MAX + 1);
    errno = 0;
    l = strtol(buf, &end, 10);
    printf("LONG_MAX + 1: %d %d %d\n", l == LONG_MAX, errno == ERANGE,
           *end == 0);
    sprintf(buf, "-%lu", (unsigned long) LONG_MAX + 1);
    errno = 0;
    l = strtol(buf, &end, 10);
    printf("LONG_MIN: %d %d\n", l == LONG_MIN, errno);
    sprintf(buf, "-%lu", (unsigned long) LONG_MAX + 2);
    errno = 0;
    l = strtol(buf, &end, 10);
    printf("LONG_MIN - 1: %d %d\n", l == LONG_MIN, errno == ERANGE);

    errno = 0;
    ll = strtoll("9223372036854775807", &end, 10);
    printf("LLONG_MAX: %d %d\n", ll == LLONG_MAX, errno);
    ll = strtoll("9223372036854775808", &end, 10);
    printf("LLONG_MAX + 1: %d %d\n", ll == LLONG_MAX, errno == ERANGE);
    errno = 0;
    ll = strtoll("-9223372036854775808", &end, 10);
    printf("LLONG_MIN: %d %d\n", ll == LLONG_MIN, errno);
    ll = strtoll("-9223372036854775809", &end, 10);
    printf("LLONG_MIN - 1: %d %d\n", ll == LLONG_MIN, errno == ERANGE);

    errno = 0;
    printf("strtoul -1: %d %d\n", strtoul("-1", &end, 10) == ULONG_MAX, errno);
    printf("strtoull -1: %d %d\n", strtoull("-1", 0, 10) == ULLONG_MAX, errno);
    printf("strtoul -huge: %d %d\n",
           strtoul("-99999999999999999999999", &end, 10) == ULONG_MAX,
           errno == ERANGE);
    errno = 0;
    printf("strtoull huge: %d %d %d\n",
           strtoull("99999999999999999999999 x", &end, 10) == ULLONG_MAX,
           errno == ERANGE, *end == ' ');
    errno = 0;
    printf("strtoull max: %d %d\n",
           strtoull("18446744073709551615", 0, 10) == ULLONG_MAX, errno);
    printf("strtoull hex: %d\n",
           strtoull("0xFFFFFFFFFFFFFFFF", 0, 0) == ULLONG_MAX);
    printf("strtoll base 36: %lld\n", strtoll("zzzzzzzzzz", 0, 36));
}

int main(void)
{
    int i, j;
    div_t d;
    ldiv_t ld;
    lldiv_t lld;

    printf("%d %d %d\n", EDOM != ERANGE, EDOM != EILSEQ, ERANGE != EILSEQ);
    errno = 5;
    printf("errno %d\n", errno);
    for (i = -2; i < 210; i++)
        if (i <= 0 || i == EDOM || i == ERANGE || i == EILSEQ || i > 200)
            printf("strerror(%d): %s\n", i, strerror(i));
    printf("%s\n", strerror(-8388608));

    for (i = 0; i < (int) (sizeof texts / sizeof *texts); i++)
        for (j = 0; j < (int) (sizeof bases / sizeof *bases); j++)
            one(i, texts[i], bases[j]);
    past();

    printf("atoll %lld %lld\n", atoll(" -1234567890123"), atoll("77x"));
    printf("llabs %lld %lld\n", llabs(-5000000000LL), llabs(7));
    for (i = -7; i <= 7; i += 7)
        for (j = -3; j <= 3; j += 6) {
            d = div(i, j);
            ld = ldiv(i * 100000L, j);
            lld = lldiv(i * 10000000000LL, j);
            printf("div %d %d: %d %d, %ld %ld, %lld %lld\n", i, j, d.quot,
                   d.rem, ld.quot, ld.rem, lld.quot, lld.rem);
        }

    /* Last, and after a flush: on the host stderr is not buffered and
     * stdout, going to a file, is. */
    fflush(stdout);
    errno = ERANGE;
    perror("perror");
    errno = EDOM;
    perror("");
    perror(NULL);

    return 0;
}
