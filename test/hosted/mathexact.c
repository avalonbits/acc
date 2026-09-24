/* The functions of <math.h> whose answers are exact, each against glibc's
 * float version bit for bit, printed with %a: fma, remainder and remquo,
 * nextafter, ilogb and logb, scalbln, and the rounding to long long.
 * The float names, so that the host's are single precision too. */
#include <math.h>
#include <stdio.h>

static const float vals[] = {
    0.0f, -0.0f, 1.0f, -1.0f, 0.5f, 1.5f, 2.5f, -2.5f, 3.0f, 7.0f, -7.25f,
    0.1f, 1e-3f, 123.456f, -98765.4f, 1e10f, 3.4e38f, 1.17549435e-38f,
    1e-40f, 1.4e-45f, 8388607.5f, 16777215.0f, 33554430.0f,
};
#define N ((int) (sizeof vals / sizeof *vals))

int main(void)
{
    int i, j, k, q;

    for (i = 0; i < N; i++) {
        float x = vals[i];

        printf("%a: ilogb %d logb %a", x, x == 0.0f ? ilogbf(x) == FP_ILOGB0
                                                     : ilogbf(x), logbf(x));
        printf(" scalbln %a %a %a", scalblnf(x, 3), scalblnf(x, -140),
               scalblnf(x, 100000L));
        if (fabsf(x) < 1e15f)
            printf(" llround %lld llrint %lld", llroundf(x), llrintf(x));
        printf(" next %a %a\n", nextafterf(x, 1e30f), nextafterf(x, -1e30f));
    }

    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++) {
            float x = vals[i], y = vals[j], r;

            if (y == 0.0f || fabsf(x) > 1e30f)
                continue;
            r = remquof(x, y, &q);
            printf("rem %a %a = %a %a q %d\n", x, y, remainderf(x, y), r, q & 7);
        }

    for (i = 0; i < N; i += 2)
        for (j = 1; j < N; j += 3)
            for (k = 0; k < N; k += 4) {
                float x = vals[i], y = vals[j], z = vals[k];

                printf("fma %a %a %a = %a\n", x, y, z, fmaf(x, y, z));
            }

    /* Where a single rounding differs from two. */
    printf("fma %a\n", fmaf(1.0f + 0x1p-12f, 1.0f - 0x1p-12f, -1.0f));
    printf("fma %a\n", fmaf(0x1.fffffep0f, 0x1.fffffep0f, -0x1.fffffcp1f));
    printf("fma %a\n", fmaf(0x1p-75f, 0x1p-75f, 0x1p-149f));
    printf("fma %a\n", fmaf(0x1.000002p0f, 0x1.000002p0f, -1.0f));
    printf("fma %a\n", fmaf(3.0f, 0x1p-150f * 1.0f, 0x1p-149f));

    printf("nextafter to itself %a, to 0 %a\n", nextafterf(2.0f, 2.0f),
           nextafterf(0x1p-149f, 0.0f));

    return 0;
}
