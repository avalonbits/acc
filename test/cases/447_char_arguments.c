/* A char argument to a char parameter pushed as the byte alone, the
 * callee widening it itself: from a byte just read, from a char local,
 * signed and unsigned, through a pointer to the function -- and with the
 * bytes above it whatever they were, which the callee must not read. An
 * argument past a prototype's parameters is still widened to int, as is
 * a _Bool's. */
#include <stdarg.h>

static int widened(signed char c) { return c; }
static int zero(unsigned char c) { return c; }
static int plain(char c) { return c; }
static int truth(_Bool b) { return b ? 7 : 3; }

static int pair(signed char a, unsigned char b)
{
    return a * 1000 + b;
}

static int sum(int n, ...)
{
    va_list ap;
    int s = 0;

    va_start(ap, n);
    while (n--)
        s += va_arg(ap, int);
    va_end(ap);

    return s;
}

int main(void)
{
    static signed char bytes[] = { -5, 120, -128 };
    static unsigned char ubytes[] = { 200, 7, 255 };
    signed char sc = -100;
    unsigned char uc = 250;
    char c = 'x';
    _Bool b = 1;
    int (*fp)(signed char) = widened;
    signed char *p = bytes;
    unsigned char *q = ubytes;

    if (widened(sc) != -100 || zero(uc) != 250 || plain(c) != 'x')
        return 1;
    if (widened(*p) != -5 || widened(p[2]) != -128 || zero(*q) != 200
        || zero(q[2]) != 255)
        return 2;
    if (pair(p[0], q[0]) != -5000 + 200 || pair(sc, uc) != -100000 + 250)
        return 3;
    if (fp(p[2]) != -128 || fp(sc) != -100)
        return 4;
    if (sum(3, p[0], sc, q[2]) != -5 - 100 + 255)
        return 5;
    if (truth(b) != 7 || truth(0) != 3)
        return 6;

    return 42;
}
