/* printf's conversions against libagon's, through snprintf so that the
 * text and the count are both seen; and snprintf(NULL, 0) counting without
 * writing a thing.
 *
 * Where libagon departs from C99 acc does not follow, and those cases are
 * in test/printf.sh, held to glibc: libagon rounds a tie away from zero
 * (%.0f of 2.5 is 3) and rounds the float's value as if it were decimal
 * (%.2f of 1.005f is 1.01); it has no %n, %lld or %g (%g is %f), and %e
 * has no exponent. */
#include <stdio.h>

#define T(...) do { char b[96]; int n = snprintf(b, sizeof b, __VA_ARGS__); \
                    printf("%3d [%s]  %s\n", n, b, #__VA_ARGS__); } while (0)

int main(void)
{
    char small[6];
    int n;

    T("%d %i %u", 42, -42, 42u);
    T("%5d|%-5d|%05d|%+d|% d|%+5d", 42, 42, 42, 42, 42, -42);
    T("%.3d|%8.3d|%-8.3d|%.0d|%5.0d", 7, -7, 7, 0, 0);
    T("%x %X %o %#x %#X %#o %#o", 255, 255, 8, 255, 255, 8, 0);
    T("%08x|%-#8x|%#08x", 0xbeef, 0xbeef, 0xbeef);
    T("%ld %lu %lx %li", -2147483647L - 1, 4294967295UL, 0xdeadbeefUL, 123456789L);
    T("%hd %hu %hhd %hhu %hx", 70000, 70000, 300, 300, 0x12345);
    T("%c|%3c|%-3c|", 'a', 'b', 'c');
    T("%s|%8s|%-8s|%.2s|%8.2s|", "abc", "abc", "abc", "abc", "abc");
    T("%s", "");
    T("%%|%5%|");
    T("%*d|%-*d|%.*d|%*.*d", 5, 1, 5, 2, 3, 3, 6, 2, 4);
    T("%d %d", 0, -1);
    T("%u %x", -1, -1);
    T("%f %f %f %f", 0.0, 1.5, -2.25, 123456.75);
    T("%.0f %.1f %.2f %.3f", 3.5, 0.375, 1.0625, 2.5);
    T("%8.3f|%-8.3f|%08.3f|%+.2f|% .2f", 3.14159, 3.14159, -3.14159, 1.0, 1.0);
    T("%f %f", 1e10, 1e-5);
    T("%#.0f %#.0f", 3.0, 0.0);
    n = snprintf(small, sizeof small, "%s", "abcdefgh");
    printf("trunc %d [%s]\n", n, small);
    n = snprintf(NULL, 0, "%d", 12345);
    printf("null %d\n", n);
    return 0;
}
