/* <math.h> against libagon's, where an answer is exact and so the same
 * from any library that gets it right: the sign of a zero from fmax,
 * fmin (of two equal, the first, as agondev's and glibc's) and hypot (never
 * -0), and the functions whose results are whole or exact.
 *
 * Where libagon departs from C99 acc does not follow, and those are held
 * to glibc in test/hosted/ and test/mathvalues.c: libagon loses the sign
 * of -0 through sin, tan, asin, atan, sqrt, cbrt, frexp, modf and ldexp;
 * answers 0 where C wants a NaN (sqrt, asin, acos of what is out of their
 * domain); has cbrt of a negative number 0, erf of one positive, logb and
 * log2 of 0 a NaN, pow(0, -1) 0, copysign(0, -1) +0, and lrint
 * truncating; has sqrt of a square inexact; and rounds most functions less
 * closely than acc. */
#include <stdio.h>
#include <string.h>
#include <math.h>

static unsigned long bits(double v)
{
    float f = (float) v;
    unsigned long u;

    memcpy(&u, &f, 4);

    return u;
}

int main(void)
{
    static const float xs[] = { 0.0f, 1.0f, 2.5f, 3.0f, 7.25f, 100.0f, 0.75f, 16.0f };
    unsigned i;
    int e;
    double ip;

    printf("fmax %08lx %08lx %08lx %08lx\n", bits(fmax(0.0, -0.0)), bits(fmax(-0.0, 0.0)),
           bits(fmax(-0.0, -0.0)), bits(fmax(2.0, -3.0)));
    printf("fmin %08lx %08lx %08lx %08lx\n", bits(fmin(0.0, -0.0)), bits(fmin(-0.0, 0.0)),
           bits(fmin(0.0, 0.0)), bits(fmin(2.0, -3.0)));
    printf("hypot %08lx %08lx %08lx %08lx\n", bits(hypot(-0.0, 0.0)), bits(hypot(-0.0, -0.0)),
           bits(hypot(3.0, 4.0)), bits(hypot(-5.0, 12.0)));
    for (i = 0; i < sizeof xs / sizeof *xs; i++) {
        double x = xs[i];

        printf("%08lx: fabs %08lx floor %08lx ceil %08lx trunc %08lx round %08lx", bits(x),
               bits(fabs(-x)), bits(floor(x)), bits(ceil(x)), bits(trunc(x)), bits(round(x)));
        printf(" ldexp %08lx frexp %08lx %d modf %08lx",
               bits(ldexp(x, 4)), bits(frexp(x, &e)), e, bits(modf(x, &ip)));
        printf(" %08lx fmod %08lx copysign %08lx fdim %08lx lround %ld\n", bits(ip),
               bits(fmod(x, 2.0)), bits(copysign(x + 1.0, -1.0)), bits(fdim(x, 2.0)), lround(x));
    }

    return 0;
}
