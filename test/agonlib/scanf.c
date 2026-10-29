/* sscanf against libagon's: each conversion, widths, what is left where it
 * stops, and the count it answers.
 *
 * Where libagon departs from C99 acc does not follow, and those cases are
 * in test/hosted/scanf.c, held to glibc: libagon answers 0 and not EOF for
 * input that ends before the first conversion, counts %n and the second
 * of a %2c as conversions, and answers more than it read for "42" against
 * "%d %d %d"; it reads no 0x for %x or %i and no 0 for %i's octal, takes a
 * sign alone as an octal 0, and has no scansets. */
#include <stdio.h>

int main(void)
{
    static const char *ins[] = { "-17 x", "abc", "12abc", "3 4 5", "9 8 7 6" };
    int a, b, c, n;
    unsigned u;
    long l;
    char s[16], t[16];
    float f;
    unsigned i;

    for (i = 0; i < sizeof ins / sizeof *ins; i++) {
        a = b = c = -99;
        n = sscanf(ins[i], "%d %d %d", &a, &b, &c);
        printf("d [%s] %d: %d %d %d", ins[i], n, a, b, c);
        a = -99;
        n = sscanf(ins[i], "%i", &a);
        printf(" | i %d: %d", n, a);
        u = 99;
        n = sscanf(ins[i], "%x", &u);
        printf(" | x %d: %u", n, u);
        u = 99;
        n = sscanf(ins[i], "%o", &u);
        printf(" | o %d: %u\n", n, u);
    }
    n = sscanf("123456", "%2d%3d", &a, &b);
    printf("width %d: %d %d\n", n, a, b);
    n = sscanf("hello world", "%s %s", s, t);
    printf("s %d: [%s] [%s]\n", n, s, t);
    n = sscanf("abcdef", "%3s", s);
    printf("s3 %d: [%s]\n", n, s);
    n = sscanf("x=12,y=7", "x=%d,y=%d", &a, &b);
    printf("literal %d: %d %d\n", n, a, b);
    n = sscanf("x=12;y=7", "x=%d,y=%d", &a, &b);
    printf("mismatch %d: %d\n", n, a);
    n = sscanf("  7", "%c", s);
    printf("c %d: %d\n", n, s[0]);
    n = sscanf("123 456", "%*d %d", &a);
    printf("skip %d: %d\n", n, a);
    n = sscanf("-2147483647", "%ld", &l);
    printf("ld %d: %ld\n", n, l);
    n = sscanf("1.5 -2.25", "%f", &f);
    printf("f %d: %d\n", n, (int) (f * 100));
    return 0;
}
