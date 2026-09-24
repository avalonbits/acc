/* What printf makes of a format, printed rather than measured.
 *
 * Every line here is written by acc's printf on the Agon and by the host's
 * on the machine running the test, and the two have to agree character for
 * character. What that pins is the part of printf that is arithmetic and
 * rules rather than machinery: where the padding goes, what a precision does
 * to a number and to a string, how a negative number is signed, what the
 * hex digits are.
 *
 * The values are all inside a 24-bit int and a 32-bit long, which is where
 * the two machines still agree about what a number is. `%p` is not here: the
 * address a pointer holds is not the same on both, and its spelling is the
 * implementation's to choose.
 *
 * Only `\n` ends a line: the host writes one byte for it and MOS's console
 * takes it, and the test strips the carriage returns MOS adds. */

#include <locale.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* A value made a float and then a double again, which is exact: what the
 * Agon's printf is given, whatever the host's double would have been. */
#define F(x) ((double) (float) (x))

static volatile float zero_f = 0.0f;

/* The v forms are reached through a va_list, so through one of these. */
static int to_buffer(char *to, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsprintf(to, fmt, ap);
    va_end(ap);

    return n;
}

static int to_screen(const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);

    return n;
}

static void hex_bytes(const char *s, int n)
{
    int i;

    printf("%d:", n);
    for (i = 0; i < n; i++)
        printf(" %02x", (unsigned char) s[i]);
    printf("\n");
}

int main(void)
{
    int n = 0;
    char buf[64];
    double inf = 1.0f / zero_f;

    printf("plain\n");
    printf("[%d] [%d] [%d]\n", 0, 42, -42);
    printf("[%5d] [%-5d] [%05d]\n", 42, 42, 42);
    printf("[%5d] [%-5d] [%05d]\n", -42, -42, -42);
    printf("[%+d] [% d] [%+d]\n", 42, 42, -42);
    printf("[%u] [%7u] [%-7u]\n", 4000000u, 12345u, 12345u);
    printf("[%x] [%X] [%08x] [%#o]\n", 48879u, 48879u, 255u, 8u);
    printf("[%o] [%o]\n", 0u, 511u);
    printf("[%ld] [%ld] [%lu]\n", 100000L, -100000L, 4000000000UL);
    printf("[%lX] [%lx]\n", 3735928559UL, 3735928559UL);
    /* The widest there is, which is the path every other number goes down
     * after being widened to it: a 64-bit divide, which this chip does by
     * calling for it. */
    printf("[%llu] [%lld]\n", 12345678901234ULL, -12345678901234LL);
    printf("[%llx] [%#llX]\n", 81985529216486895ULL, 81985529216486895ULL);
    printf("[%s] [%10s] [%-10s]\n", "abc", "abc", "abc");
    printf("[%.2s] [%.0s] [%.10s]\n", "abcdef", "abcdef", "abc");
    printf("[%*d] [%-*d] [%.*d]\n", 6, 42, 6, 42, 5, 42);
    printf("[%.*s] [%*s]\n", 3, "abcdef", 5, "ab");
    printf("[%c] [%c] [%%]\n", 65, 122);
    printf("[%.0d] [%.0d] [%.3d]\n", 0, 7, 7);
    printf("[%d%s%d]\n", 1, "-", 2);
    printf("[%6.3d] [%-6.3d]\n", 5, 5);

    /* What it answers with is the number of characters it wrote. */
    n = printf("");
    printf("wrote %d\n", n);
    n = printf("%5s|", "ab");
    printf("\nwrote %d\n", n);
    n = printf("%ld", -1234567L);
    printf("\nwrote %d\n", n);

    /* Floating point, every value made a float first, so that the host
     * prints exactly the value the Agon has: its double is the same number.
     * Rounding is to the nearest with a tie to even, on the exact value. */
    printf("[%f] [%f] [%f] [%f]\n", F(1.5), F(-0.25), F(0.0), F(3.14159));
    printf("[%f] [%f] [%lf]\n", F(1e10), F(1e-5), F(123456.789));
    printf("[%.10f] [%.20f] [%.3f]\n", F(0.1), F(0.1), F(2.0625));
    printf("[%.0f] [%.0f] [%.0f] [%.0f] [%.1f]\n", F(0.5), F(1.5), F(2.5), F(-2.5), F(0.25));
    printf("[%.0f] [%.2f] [%.1f]\n", F(3.4e38), F(9.995), F(99.95));
    printf("[%e] [%e] [%E] [%e]\n", F(1.5e-10), F(12345.678), F(0.0), F(-7.0));
    printf("[%.2e] [%.1e] [%.0e] [%.3e]\n", F(9.9999), F(0.0999), F(2.5), F(1e-40));
    printf("[%g] [%g] [%g] [%g] [%g]\n", F(100000.0), F(1e6), F(1e-4), F(1e-5), F(123.456));
    printf("[%g] [%G] [%.3g] [%#g] [%g] [%.0g]\n", F(0.0001234), F(1e-10), F(2.5), F(1.5), F(0.0), F(123.0));
    printf("[%a] [%a] [%A] [%a] [%a]\n", F(1.0), F(0.1), F(-3.0), F(1e-40), F(0.0));
    printf("[%.2a] [%.0a] [%#a] [%.8a]\n", F(1.999), F(1.5), F(2.0), F(1.0));
    printf("[%f] [%e] [%F] [%+g]\n", inf, -inf, inf, inf);
    printf("[%+.2f] [% .1f] [%08.2f] [%-10.1e|] [%#.0f] [%10.3g|]\n",
           F(3.5), F(3.5), F(-3.5), F(3.5), F(3.0), F(0.5));
    printf("[%010.2e] [%-+8.1f|] [%012a]\n", F(-1.5), F(2.25), F(1.5));
    n = printf("%.3f", F(1.0));
    printf("\nwrote %d\n", n);

    /* And a sweep: floats made from random bits, over every exponent there
     * is, subnormals included and infinities and NaNs left out, each
     * written four ways. The bits come from a generator both machines run
     * the same, so the two print the same values. */
    {
        unsigned long seed = 12345;
        int i;

        for (i = 0; i < 64; i++) {
            union { float f; uint32_t u; } v;

            seed = (seed * 1103515245UL + 12345UL) & 0xffffffffUL;
            v.u = (uint32_t) (seed ^ (seed >> 13) << 7);
            if (((v.u >> 23) & 0xff) == 0xff)
                v.u &= 0xbfffffffUL;
            printf("%.9e %.9g %a %.12f\n", (double) v.f, (double) v.f,
                   (double) v.f, (double) v.f);
        }
    }

    /* sprintf and vsprintf: what lands in the buffer, its terminator, and
     * the count, which does not include the terminator. The byte past it is
     * set first, so that a terminator written one place late shows. */
    buf[6] = 'x';
    buf[7] = 'y';
    n = sprintf(buf, "%d|%s", -42, "ab");
    printf("sprintf [%s] %d %d %c\n", buf, n, buf[6], buf[7]);
    n = sprintf(buf, "");
    printf("sprintf [%s] %d\n", buf, n);
    n = to_buffer(buf, "[%5.2s] [%-4x] [%+05d]", "abcdef", 255u, 7);
    printf("vsprintf [%s] %d\n", buf, n);
    n = to_screen("vprintf [%c%c] [%lu]\n", 'o', 'k', 4000000000UL);
    printf("vprintf wrote %d\n", n);

    /* The lengths that narrow: h and hh cut an int down to a short or a
     * char before it is written; j, z and t are intmax_t, size_t and
     * ptrdiff_t. And %n, at each width, counting what went before it. */
    printf("[%hd] [%hhd] [%hu] [%hhu] [%hx] [%hhx] [%hho]\n", 70000, 300, 70000,
           300, -1, -1, 511);
    printf("[%hhd] [%hd] [%hhi]\n", 200, 40000, -129);
    printf("[%jd] [%ju] [%jx] [%zu] [%zx] [%td] [%ti]\n", (intmax_t) -5000000000LL,
           (uintmax_t) 5000000000ULL, (uintmax_t) 0xabcdef012ULL, (size_t) 12345,
           (size_t) 255, (ptrdiff_t) -77, (ptrdiff_t) 99);
    {
        int ni = 0;
        long nl = 0;
        long long nll = 0;
        short ns = 0;
        signed char nc = 0;

        printf("abc%nde%lnf%lln%hn%hhn|\n", &ni, &nl, &nll, &ns, &nc);
        printf("n %d %ld %lld %d %d\n", ni, nl, nll, ns, nc);
    }

    /* %lc and %ls: wide characters written as UTF-8, with widths and
     * precisions in bytes and never part of a character. The host is put in
     * its UTF-8 locale for these, which is what acc's C locale is. Shown as
     * the bytes, since the emulator's screen shows each byte past 127 as a
     * character of its own. */
    setlocale(LC_CTYPE, "C.UTF-8");
    n = snprintf(buf, sizeof buf, "[%lc] [%3lc] [%-3lc] [%lc]", 'A', 0xe9, 'z', 0x20ac);
    hex_bytes(buf, n);
    n = snprintf(buf, sizeof buf, "[%ls] [%8ls] [%-8ls] [%.3ls] [%.4ls] [%.1ls]",
                 L"plain", L"caf\u00e9", L"caf\u00e9", L"caf\u00e9", L"caf\u00e9",
                 L"\u20ac5");
    hex_bytes(buf, n);
    printf("[%ls] [%.2ls]\n", L"", L"ascii");

    return 0;
}
