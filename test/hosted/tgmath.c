/* <tgmath.h>: the generic names, with integer, float and double
 * arguments, through functions whose answers are exact so that the host's
 * double and float functions and the Agon's single one all agree. */
#include <stdio.h>
#include <tgmath.h>

int main(void)
{
    float f = 2.25f;
    double d = -7.5;
    int i = 9, q;

    printf("%a %a %a\n", (float) sqrt(i), (float) sqrt(f), (float) sqrt(4.0));
    printf("%a %a %a\n", (float) fabs(-3), (float) fabs(d), (float) floor(f));
    printf("%a %a %a\n", (float) ceil(d), (float) trunc(-f), (float) round(d));
    printf("%a %a\n", (float) fmax(i, f), (float) fmin(d, 2));
    printf("%a %a\n", (float) fmod(i, 4), (float) copysign(3, d));
    printf("%a %a\n", (float) ldexp(f, 3), (float) scalbn(i, -2));
    printf("%d %ld %lld\n", ilogb(1024), lround(f), llround(d));
    printf("%a %d\n", (float) remquo(i, 4.0f, &q), q);
    printf("%a\n", (float) fma(f, 2, i));
    printf("float stays float %d\n", sizeof sqrt(f) == sizeof (float));
    printf("int becomes double %d\n", sizeof sqrt(i) == sizeof (double));
#ifdef sin
    printf("sin is generic\n");
#endif

    return 0;
}
