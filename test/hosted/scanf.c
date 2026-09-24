/* The scanf family against glibc's: every conversion and length, widths,
 * suppression, scan sets, %n, what is left unread after a failure, and
 * what each returns -- including EOF when the input runs out first. */
#include <inttypes.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void ints(const char *in, const char *fmt)
{
    int a = -1, b = -1, c = -1, n = -1;
    int r = sscanf(in, fmt, &a, &b, &c, &n);

    printf("[%s] [%s] -> %d: %d %d %d %d\n", in, fmt, r, a, b, c, n);
}

static void floats(const char *in, const char *fmt)
{
    float a = -1, b = -1;
    int n = -1;
    int r = sscanf(in, fmt, &a, &b, &n);

    printf("[%s] [%s] -> %d: %a %a %d\n", in, fmt, r, a, b, n);
}

static void strings(const char *in, const char *fmt)
{
    char a[32], b[32];
    int n = -1, r;

    strcpy(a, "-");
    strcpy(b, "-");
    r = sscanf(in, fmt, a, b, &n);
    printf("[");
    for (; *in; in++)
        putchar(*in < ' ' ? '?' : *in);
    printf("] [%s] -> %d: [%s] [%s] %d\n", fmt, r, a, b, n);
}

int main(void)
{
    /* UTF-8, which is what acc's C locale is; the host has to be asked.
     * acc's setlocale refuses the name, and nothing changes. */
    setlocale(LC_CTYPE, "C.UTF-8");

    ints("12 34 56", "%d %d %d%n");
    ints("  -7,+8;9", "%d,%d;%d%n");
    ints("0x1f 017 99", "%i %i %i%n");
    ints("0x1f 017 99", "%x %o %u%n");
    ints("ff FF 0XfF", "%x %X %x%n");
    ints("123456", "%2d%3d%d%n");
    ints("12abc", "%d%d%d%n");
    ints("", "%d%d%d%n");
    ints("   ", "%d%d%d%n");
    ints("x", "%d%d%d%n");
    ints("5 x", "%d %d%d%n");
    ints("1 2", "%*d %d%n%d");
    ints("10%20", "%d%%%d%n%d");
    ints("10 % 20", "%d %% %d%n%d");
    ints("-0", "%u%n%d%d");
    ints("08", "%i%d%n%d");
    ints("0x", "%x%n%d%d");
    ints("abc", "abd%d%n%d%d");
    ints("  42", "%n%d%n%d");

    {
        signed char hh;
        short h;
        long l;
        long long ll;
        intmax_t j;
        size_t z;
        ptrdiff_t t;
        void *p;
        int r = sscanf("300 70000 -2000000000 -9000000000000000000 123456789012 77 -5 1234",
                       "%hhd %hd %ld %lld %jd %zu %td %p", &hh, &h, &l, &ll, &j, &z, &t, &p);

        printf("%d: %d %d %ld %lld %" PRIdMAX " %u %d %d\n", r, hh, h, l, ll, j,
               (unsigned) z, (int) t, p == (void *) 0x1234);
    }
    {
        int32_t a;
        uint16_t b;
        int64_t c;
        int r = sscanf("-5 65535 -77", "%" SCNd32 " %" SCNu16 " %" SCNd64, &a, &b, &c);

        printf("SCN %d: %ld %u %lld\n", r, (long) a, (unsigned) b, (long long) c);
    }

    floats("1.5 -2.25e3", "%f %e%n");
    floats("  .5x", "%g%g%n");
    /* Not "1e+" or "1e": C99 7.19.6.2 says neither is a number and the
     * conversion fails, and glibc takes each as 1. acc does what C says. */
    floats("1e5 x", "%f %f%n");
    floats("0x1.8p1 inf", "%a %f%n");
    floats("-INFINITY nan", "%f %f%n");
    floats("123456789", "%4f%f%n");
    floats("3.402823e38 1e-45", "%f%f%n");
    floats("+.e1", "%f%f%n");
    {
        double d;
        int r = sscanf("0.1", "%lf", &d);

        printf("lf: %d %a\n", r, (float) d);
    }

    strings("hello world", "%s %s%n");
    strings("  hi\vthere\n", "%s%s%n");
    strings("abcdef", "%3s%s%n");
    strings("abc,def", "%[^,],%s%n");
    strings("aaab-cd", "%[a]%[a-z-]%n");
    strings("]]x", "%[]]%s%n");
    strings("xyz", "%[abc]%s%n");
    strings("abcdef", "%3c%2c%n");
    /* Not %3c of "ab": C says that is a matching failure, and 0, and glibc
     * counts it as one; see test/lib.sh. */
    strings("abcd", "%3c%s%n");
    strings(" q", "%c%s%n");
    strings(" q r", " %c %s%n");
    strings("", "%s%s%n");

    {
        wchar_t w[8];
        wchar_t wc = 0;
        int r = sscanf("h\xc3\xa9llo z", "%ls %lc", w, &wc);

        printf("%%ls: %d %d %d %d %d\n", r, w[1] == 0xe9, w[4] == 'o', w[5] == 0,
               wc == 'z');
    }

    /* From a file, which gives back through ungetc. */
    {
        FILE *f = fopen("t_scan.txt", "w");
        int a = 0, b = 0, r;
        char s[16];

        fputs("17 apples, 23 pears\nleft", f);
        fclose(f);
        f = fopen("t_scan.txt", "r");
        r = fscanf(f, "%d %s %d", &a, s, &b);
        printf("fscanf %d: %d [%s] %d", r, a, s, b);
        r = fscanf(f, "%d", &a);
        printf(", then %d, next [%c]", r, fgetc(f));
        fscanf(f, "%s", s);
        r = fscanf(f, "%s", s);
        printf(", [%s] then %d\n", s, r);
        fclose(f);
        remove("t_scan.txt");
    }

    return 0;
}
