/* <math.h> at the edges: what each function answers for zeros,
 * infinities, NaNs and arguments outside its domain, and what it leaves in
 * errno, against glibc's float functions. math_errhandling says errno is
 * how errors are reported, and C99 7.12.1 says which errors are which: a
 * domain error sets EDOM, and a pole or an overflow sets ERANGE. */
#include <errno.h>
#include <math.h>
#include <stdio.h>

static const char *err(void)
{
    return errno == 0 ? "-" : errno == EDOM ? "EDOM" : errno == ERANGE ? "ERANGE"
         : "other";
}

/* A NaN's sign and payload are not what is being tested. */
static void show(const char *what, float v)
{
    if (isnan(v))
        printf("%s = nan %s\n", what, err());
    else
        printf("%s = %a %s\n", what, v, err());
}

/* Through volatiles, so that the host's compiler cannot work the answer
 * out itself and leave errno alone. */
static volatile float va, vb;
static volatile int vi;

#define ONE(f, x)    do { va = (x); errno = 0; show(#f "(" #x ")", f(va)); } while (0)
#define TWO(f, x, y) do { va = (x); vb = (y); errno = 0; \
                          show(#f "(" #x ", " #y ")", f(va, vb)); } while (0)
#define INT(f, x, n) do { va = (x); vi = (n); errno = 0; \
                          show(#f "(" #x ", " #n ")", f(va, vi)); } while (0)

int main(void)
{
    float inf = INFINITY, nan = NAN, big = 3e38f;

    printf("math_errhandling %d\n", (math_errhandling & MATH_ERRNO) != 0);

    ONE(sqrtf, -1.0f); ONE(sqrtf, -inf); ONE(sqrtf, -0.0f); ONE(sqrtf, nan);
    ONE(logf, 0.0f); ONE(logf, -0.0f); ONE(logf, -1.0f); ONE(logf, inf);
    ONE(logf, nan); ONE(logf, -inf);
    ONE(log2f, 0.0f); ONE(log2f, -2.0f); ONE(log10f, 0.0f); ONE(log10f, -2.0f);
    ONE(log1pf, -1.0f); ONE(log1pf, -2.0f); ONE(log1pf, inf);
    ONE(expf, 89.0f); ONE(expf, 1000.0f); ONE(expf, inf); ONE(expf, -inf);
    ONE(expf, nan); ONE(exp2f, 128.0f); ONE(exp2f, 200.0f); ONE(expm1f, 89.0f);
    ONE(expm1f, -inf);
    ONE(sinhf, 89.0f); ONE(sinhf, -89.0f); ONE(sinhf, 100.0f);
    ONE(coshf, 89.0f); ONE(coshf, 100.0f); ONE(coshf, -inf);
    ONE(asinf, 2.0f); ONE(asinf, -1.5f); ONE(acosf, 2.0f); ONE(asinf, nan);
    ONE(acoshf, 0.5f); ONE(acoshf, 1.0f); ONE(atanhf, 1.0f); ONE(atanhf, -1.0f);
    ONE(atanhf, 2.0f);
    ONE(sinf, inf); ONE(cosf, -inf); ONE(tanf, inf); ONE(sinf, nan);
    ONE(tgammaf, 0.0f); ONE(tgammaf, -0.0f); ONE(tgammaf, -1.0f);
    ONE(tgammaf, -inf); ONE(tgammaf, 36.0f); ONE(tgammaf, 35.2f);
    ONE(lgammaf, 0.0f); ONE(lgammaf, -2.0f); ONE(lgammaf, inf); ONE(lgammaf, 1e37f);
    ONE(logbf, 0.0f); ONE(logbf, inf);
    TWO(powf, 0.0f, -1.0f); TWO(powf, -0.0f, -3.0f); TWO(powf, -0.0f, -2.0f);
    TWO(powf, -2.0f, 0.5f); TWO(powf, 10.0f, 39.0f); TWO(powf, -10.0f, 39.0f);
    TWO(powf, 2.0f, 128.0f); TWO(powf, nan, 0.0f); TWO(powf, 1.0f, nan);
    TWO(fmodf, 1.0f, 0.0f); TWO(fmodf, inf, 1.0f); TWO(fmodf, nan, 1.0f);
    TWO(remainderf, 1.0f, 0.0f); TWO(remainderf, inf, 2.0f);
    TWO(hypotf, big, big); TWO(hypotf, inf, nan);
    INT(ldexpf, big, 10); INT(scalbnf, 1.0f, 200);
    TWO(nextafterf, 0x1.fffffep127f, inf);
    va = big;
    errno = 0;
    show("fmaf(big, 10, 0)", fmaf(va, 10.0f, 0.0f));
    va = 0.0f;
    errno = 0;
    printf("ilogbf(0) %d %s\n", ilogbf(va) == FP_ILOGB0, err());

    /* The quiet comparisons, over a NaN, the infinities and numbers, with
     * an argument that counts how often it is evaluated. */
    {
        static const float xs[] = { 1.0f, 2.0f, -1.0f, 0.0f };
        float vals[6];
        int i, j, n = 0;

        vals[0] = 1.0f; vals[1] = 2.0f; vals[2] = -0.0f;
        vals[3] = inf; vals[4] = -inf; vals[5] = nan;
        for (i = 0; i < 6; i++)
            for (j = 0; j < 6; j++)
                printf("%d%d%d%d%d%d ", isgreater(vals[i], vals[j]),
                       isgreaterequal(vals[i], vals[j]), isless(vals[i], vals[j]),
                       islessequal(vals[i], vals[j]), islessgreater(vals[i], vals[j]),
                       isunordered(vals[i], vals[j]));
        printf("\n");
        i = 0;
        printf("once %d", isgreater(xs[i++], xs[n++]));
        printf(" %d %d\n", i, n);
        printf("float_t %d double_t %d\n", (int) sizeof (float_t) >= (int) sizeof (float),
               (int) sizeof (double_t) >= (int) sizeof (float));
    }

    return 0;
}
