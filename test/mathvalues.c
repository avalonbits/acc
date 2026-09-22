/* What every one of these should answer, worked out to more digits than
 * a float holds and rounded to one. Generated; the generator and the
 * measurements are in the commit that added them. */
#include <math.h>

static unsigned long ub(double x)
{
    union { float f; unsigned long u; } v;

    v.f = (float) x;

    return v.u;
}

static double fb(unsigned long u)
{
    union { float f; unsigned long u; } v;

    v.u = u;

    return v.f;
}

/* How many representable steps apart two values are. Both have the same
 * sign in every case here, so their bit patterns count up the way the
 * numbers do. */
static long apart(unsigned long a, unsigned long c)
{
    if ((a ^ c) & 0x80000000UL)
        return 1000;

    return (long) (a > c ? a - c : c - a);
}

typedef double (*fn1)(double);
typedef double (*fn2)(double, double);

static int check1(fn1 f, const unsigned long *in, const unsigned long *want,
                  int n, int tol)
{
    int i, bad = 0;

    for (i = 0; i < n; i++)
        if (apart(ub(f(fb(in[i]))), want[i]) > tol)
            bad++;

    return bad;
}

static int check2(fn2 f, const unsigned long *in, const unsigned long *want,
                  int n, int tol)
{
    int i, bad = 0;

    for (i = 0; i < n; i++)
        if (apart(ub(f(fb(in[2 * i]), fb(in[2 * i + 1]))), want[i]) > tol)
            bad++;

    return bad;
}

static const unsigned long exp_in[] = {
    0x4287f4f6UL, 0x428f8dc9UL, 0x427b2847UL, 0xc2854387UL, 0x416b96f7UL, 0xc14334e6UL, 0x409a0cf5UL, 0xc26c9b2bUL, 0xc2451f30UL, 0xc10de443UL, 0xc23288f1UL, 0xc0e63b33UL, 0x00000000UL, 0x3f800000UL, 0xbf800000UL, 0x3f000000UL
};
static const unsigned long exp_want[] = {
    0x70869097UL, 0x733bb0caUL, 0x6cc01e95UL, 0x0f6a05c5UL, 0x4a1774c6UL, 0x36a8b95fUL, 0x42f67799UL, 0x14ca959bUL, 0x1bef688aUL, 0x3913a187UL, 0x1f42f94bUL, 0x3a44bf71UL, 0x3f800000UL, 0x402df854UL, 0x3ebc5ab2UL, 0x3fd3094cUL
};
static const unsigned long exp2_in[] = {
    0xc2e41ca3UL, 0xc2c6d415UL, 0x42499b9eUL, 0xc1973401UL, 0x40420e9dUL, 0x4260d739UL, 0xc2075d0eUL, 0xc2d45fb3UL, 0x4287e39eUL, 0x41ab1df9UL, 0x00000000UL, 0x3f800000UL, 0x41200000UL
};
static const unsigned long exp2_want[] = {
    0x067643e4UL, 0x0dc01bc4UL, 0x58a920b7UL, 0x36092652UL, 0x4102e23cUL, 0x5b94130cUL, 0x2e8eed19UL, 0x0a60e44fUL, 0x61765990UL, 0x4a27b01dUL, 0x3f800000UL, 0x40000000UL, 0x44800000UL
};
static const unsigned long expm1_in[] = {
    0x3e918848UL, 0x3e5f02eeUL, 0x3f58ea30UL, 0xbe7ce69dUL, 0x3eef6b5aUL, 0xbe720ac0UL, 0x3e04cbf0UL, 0x3e93f247UL, 0x00000000UL, 0x3727c5acUL, 0xb727c5acUL, 0x40a00000UL
};
static const unsigned long expm1_want[] = {
    0x3ea852a8UL, 0x3e7928aeUL, 0x3faaac9fUL, 0xbe601739UL, 0x3f189f87UL, 0xbe579025UL, 0x3e0dcac4UL, 0x3eab89c2UL, 0x00000000UL, 0x3727c5e3UL, 0xb727c575UL, 0x431369c5UL
};
static const unsigned long log_in[] = {
    0x2d00772aUL, 0x162c87eeUL, 0x3ce3b4d9UL, 0x552ce2dfUL, 0x48856df5UL, 0x3a38588dUL, 0x4da65f04UL, 0x1fc53061UL, 0x1f73884bUL, 0x2ddf5f7fUL, 0x5ef8bf84UL, 0x2120e21eUL, 0x3f800000UL, 0x40000000UL, 0x3f000000UL
};
static const unsigned long log_want[] = {
    0xc1cd247dUL, 0xc264ee41UL, 0xc0654d84UL, 0x41f0d8f3UL, 0x41484a84UL, 0xc0e85092UL, 0x419d5ccdUL, 0xc22fb797UL, 0xc231a538UL, 0xc1c32c01UL, 0x422e8ee2UL, 0xc228369cUL, 0x00000000UL, 0x3f317218UL, 0xbf317218UL
};
static const unsigned long log2_in[] = {
    0x190b8176UL, 0x177f34d4UL, 0x11896437UL, 0x2919f789UL, 0x46b6da8bUL, 0x5f710d5dUL, 0x2e9b1146UL, 0x3281c90fUL, 0x3f800000UL, 0x41000000UL
};
static const unsigned long log2_want[] = {
    0xc299c06bUL, 0xc2a0024bUL, 0xc2b7cbb3UL, 0xc232ef21UL, 0x41683b94UL, 0x427fa71eUL, 0xc206e49bUL, 0xc1cfd713UL, 0x00000000UL, 0x40400000UL
};
static const unsigned long log10_in[] = {
    0x2a6561c7UL, 0x2c08573fUL, 0x5b208354UL, 0x44598b67UL, 0x27669058UL, 0x184f1daeUL, 0x5bbdb3f8UL, 0x541c2a95UL, 0x3f800000UL, 0x42c80000UL
};
static const unsigned long log10_want[] = {
    0xc14b0e18UL, 0xc13b6771UL, 0x41853d56UL, 0x403c228aUL, 0xc167eb10UL, 0xc1bc9435UL, 0x41883a75UL, 0x4146db93UL, 0x00000000UL, 0x40000000UL
};
static const unsigned long log1p_in[] = {
    0x3fc05d0bUL, 0xbe56c1beUL, 0x3f2e904aUL, 0x3fe1d664UL, 0x3f519e08UL, 0x3febc1d4UL, 0xbed473e5UL, 0x3ff6b8efUL, 0x00000000UL, 0x358637bdUL
};
static const unsigned long log1p_want[] = {
    0x3f6adc6cUL, 0xbe710571UL, 0x3f051953UL, 0x3f8226bfUL, 0x3f1922c2UL, 0x3f85b0beUL, 0xbf093abaUL, 0x3f897debUL, 0x00000000UL, 0x358637b9UL
};
static const unsigned long sin_in[] = {
    0xc2b1a1d0UL, 0xc1860aeeUL, 0xc2bea7a6UL, 0x4277a68fUL, 0x4215c3a8UL, 0x42bd7e88UL, 0x4273f893UL, 0x42b38800UL, 0x40f703e3UL, 0x42c3497bUL, 0x42473624UL, 0xc1f62c03UL, 0x00000000UL, 0x3f800000UL, 0xbf800000UL
};
static const unsigned long sin_want[] = {
    0xbf409265UL, 0x3f5db99bUL, 0xbf61bd71UL, 0xbf4b8cb2UL, 0xbe82a7e5UL, 0x3ef52cafUL, 0xbf76d72bUL, 0x3f793eb3UL, 0x3f7dadd9UL, 0xbe80b9c4UL, 0xbee47f98UL, 0x3f19ca9fUL, 0x00000000UL, 0x3f576aa4UL, 0xbf576aa4UL
};
static const unsigned long cos_in[] = {
    0x42bdc1a7UL, 0xc1c4cd25UL, 0xc2755951UL, 0xc2a9a630UL, 0xc0d9debdUL, 0xc272af1fUL, 0x42ad6e67UL, 0x42b42e75UL, 0xc2633d37UL, 0xc29e993fUL, 0x40b9686dUL, 0xc28420c6UL, 0x00000000UL, 0x3f800000UL, 0xbf800000UL
};
static const unsigned long cos_want[] = {
    0x3f4ec99aUL, 0x3f5c8b7eUL, 0x3d9bd67bUL, 0xbf7fffebUL, 0x3f5d7d83UL, 0xbf0e6fdbUL, 0x3ea1f2afUL, 0xbf06f903UL, 0x3f77528eUL, 0xbf39a5c3UL, 0x3f61f997UL, 0xbf7ef36cUL, 0x3f800000UL, 0x3f0a5140UL, 0x3f0a5140UL
};
static const unsigned long tan_in[] = {
    0x3dfa4c0aUL, 0x3f603ee5UL, 0xbf310141UL, 0xbe605a93UL, 0x3f8fabfcUL, 0xbe1e93ddUL, 0xbee16561UL, 0x3f8c9c55UL, 0x3f60e678UL, 0x3f8da213UL, 0x00000000UL, 0x3f000000UL
};
static const unsigned long tan_want[] = {
    0x3dfb8cfcUL, 0x3f9991bdUL, 0xbf53e675UL, 0xbe640394UL, 0x40050bf0UL, 0xbe1fdb87UL, 0xbef12e6eUL, 0x3ffa925cUL, 0x3f9a5ec2UL, 0x3fff9742UL, 0x00000000UL, 0x3f0bda7bUL
};
static const unsigned long atan_in[] = {
    0x4196320aUL, 0xc10bcdf2UL, 0x419a6b19UL, 0x4203ed87UL, 0x415f382dUL, 0xc1d142baUL, 0xc1a3cc26UL, 0xc1109d1eUL, 0x41d95521UL, 0x40b6b0a5UL, 0x00000000UL, 0x3f800000UL, 0xbf800000UL
};
static const unsigned long atan_want[] = {
    0x3fc24026UL, 0xbfba79f3UL, 0x3fc26fc0UL, 0x3fc52ea5UL, 0x3fbfe71cUL, 0xbfc42bc0UL, 0xbfc2d0b6UL, 0xbfbaf51aUL, 0x3fc45a36UL, 0x3fb2dddaUL, 0x00000000UL, 0x3f490fdbUL, 0xbf490fdbUL
};
static const unsigned long asin_in[] = {
    0x3dcc8b42UL, 0x3f273060UL, 0xbf4879bbUL, 0x3e866366UL, 0xbeab8484UL, 0x3df31e73UL, 0x3f2bb2edUL, 0xbf00d7faUL, 0x3f672d5fUL, 0xbf57c4f7UL, 0x00000000UL, 0x3f800000UL, 0xbf800000UL, 0x3f000000UL
};
static const unsigned long asin_want[] = {
    0x3dcce2b4UL, 0x3f362e7fUL, 0xbf664f45UL, 0x3e87fb34UL, 0xbeaee63dUL, 0x3df3b190UL, 0x3f3c32d6UL, 0xbf07043cUL, 0x3f903a83UL, 0xbf8053c0UL, 0x00000000UL, 0x3fc90fdbUL, 0xbfc90fdbUL, 0x3f060a92UL
};
static const unsigned long acos_in[] = {
    0x3dfe0a07UL, 0x3dab461eUL, 0xbf07a384UL, 0x3df93bfaUL, 0xbd6fcdfdUL, 0xbdce7586UL, 0x3edf9f0fUL, 0xbf7dfb51UL, 0xbe7b8ab5UL, 0x3f123a4aUL, 0x00000000UL, 0x3f800000UL, 0xbf800000UL, 0x3f000000UL
};
static const unsigned long acos_want[] = {
    0x3fb924bbUL, 0x3fbe5844UL, 0x400844edUL, 0x3fb97232UL, 0x3fd08f64UL, 0x3fd5fcd2UL, 0x3f8f34e8UL, 0x40410527UL, 0x3fe8d46dUL, 0x3f767bd3UL, 0x3fc90fdbUL, 0x00000000UL, 0x40490fdbUL, 0x3f860a92UL
};
static const unsigned long sinh_in[] = {
    0xc265351cUL, 0x426b214eUL, 0xc23db3b7UL, 0x41e7d7bfUL, 0xc2512c93UL, 0xc101bb07UL, 0xc146457aUL, 0xc285d2f7UL, 0x00000000UL, 0x3dcccccdUL, 0xbdcccccdUL, 0x3f800000UL
};
static const unsigned long sinh_want[] = {
    0xe84b8857UL, 0x695faca2UL, 0xe12b518dUL, 0x53e06142UL, 0xe4ae14c2UL, 0xc4cf976cUL, 0xc7eb3674UL, 0xef394b2aUL, 0x00000000UL, 0x3dcd243aUL, 0xbdcd243aUL, 0x3f966cfeUL
};
static const unsigned long cosh_in[] = {
    0x41ff7f1cUL, 0xc04b0239UL, 0x41f1eeacUL, 0x40c85ce3UL, 0x410f0783UL, 0x4085c4c8UL, 0xc2016958UL, 0x427f33c6UL, 0x00000000UL, 0x3f800000UL
};
static const unsigned long cosh_want[] = {
    0x5606df32UL, 0x413f2db6UL, 0x54c5fea8UL, 0x4382fafeUL, 0x456e50edUL, 0x4202cc39UL, 0x564c6953UL, 0x6d04085fUL, 0x3f800000UL, 0x3fc583abUL
};
static const unsigned long tanh_in[] = {
    0xc1045ae2UL, 0x40f108c9UL, 0x40e89df4UL, 0xbd33b159UL, 0x40b13f33UL, 0x405de8cbUL, 0x40067e80UL, 0xc0855be3UL, 0x00000000UL, 0x3dcccccdUL, 0x40a00000UL
};
static const unsigned long tanh_want[] = {
    0xbf7ffffeUL, 0x3f7ffff6UL, 0x3f7ffff0UL, 0xbd3393dcUL, 0x3f7ffdfaUL, 0x3f7f8088UL, 0x3f787525UL, 0xbf7fe08dUL, 0x00000000UL, 0x3dcc1ebcUL, 0x3f7ffa0dUL
};
static const unsigned long cbrt_in[] = {
    0xdffa4998UL, 0xdfd106eaUL, 0xe009455bUL, 0x609f7dcdUL, 0xdfdc45bfUL, 0xdf9c0e30UL, 0xdf5f4472UL, 0x605b9c9dUL, 0xde76ed50UL, 0x5f153154UL, 0x00000000UL, 0x3f800000UL, 0x41000000UL, 0xc1d80000UL
};
static const unsigned long cbrt_want[] = {
    0xca49aa24UL, 0xca3de94aUL, 0xca4ffaa0UL, 0x4a89bcd1UL, 0xca4141f6UL, 0xca2c4880UL, 0xca1a14acUL, 0x4a733ee7UL, 0xc9c8c20cUL, 0x4a06b4eaUL, 0x00000000UL, 0x3f800000UL, 0x40000000UL, 0xc0400000UL
};
static const unsigned long sqrt_in[] = {
    0x2719ac5dUL, 0x2959a5cdUL, 0x16891284UL, 0x23483065UL, 0x27f56fb1UL, 0x62945ee0UL, 0x3fb40df3UL, 0x18a28453UL, 0x00000000UL, 0x3f800000UL, 0x40800000UL
};
static const unsigned long sqrt_want[] = {
    0x33465814UL, 0x346c0bd3UL, 0x2b047561UL, 0x3162618fUL, 0x33b13ec3UL, 0x5109cf36UL, 0x3f97cff3UL, 0x2c103ac4UL, 0x00000000UL, 0x3f800000UL, 0x40000000UL
};
static const unsigned long pow_in[] = {
    0x3eb6c955UL, 0xc0b4f300UL, 0x37ed0d12UL, 0x40acc433UL, 0x40c65e56UL, 0xc0d1ba80UL, 0x3e4ef874UL, 0x4073fc26UL, 0x469bab4fUL, 0xbfd58e15UL, 0x3aa959c3UL, 0x4037bfc3UL, 0x467d9bd5UL, 0xc053e689UL, 0x3a755768UL, 0xc0d7ec1fUL, 0x491702c9UL, 0x40d49127UL, 0x4223029aUL, 0x401e773dUL, 0x365a3d93UL, 0xc0a91f5fUL, 0x4597b0d5UL, 0x40f3cf69UL, 0x3b051e60UL, 0x40d2d9c6UL, 0x3cb61c83UL, 0xc0a281f4UL, 0x40000000UL, 0x41200000UL, 0x41200000UL, 0x40400000UL, 0x42000000UL, 0x3eaaaaabUL
};
static const unsigned long pow_want[] = {
    0x43a937e9UL, 0x16aadec3UL, 0x36d734afUL, 0x3b13aa14UL, 0x339024d8UL, 0x31aeb39fUL, 0x284eba4bUL, 0x616d28f8UL, 0x7f5e3e90UL, 0x4617916cUL, 0x6fa2f05aUL, 0x6e1d1619UL, 0x22067498UL, 0x4d6cb3e8UL, 0x44800000UL, 0x447a0000UL, 0x404b2ff6UL
};
static const unsigned long atan2_in[] = {
    0xc2c05c2bUL, 0x41923ca5UL, 0x41b198beUL, 0xc20a8341UL, 0xc29056deUL, 0xc2925d2eUL, 0x42398315UL, 0xc2a3011eUL, 0xc0c7322bUL, 0x428ec70dUL, 0x42274057UL, 0xc26b20dbUL, 0x415f551aUL, 0xc16afe69UL, 0x425890cfUL, 0xc212998eUL, 0x4280a6b6UL, 0x41bf6c5cUL, 0x41a4a549UL, 0x40d56ab9UL, 0x3f800000UL, 0x00000000UL, 0x00000000UL, 0x3f800000UL, 0xbf800000UL, 0xbf800000UL
};
static const unsigned long atan2_want[] = {
    0xbfb1057eUL, 0x402493a1UL, 0xc0173e00UL, 0x4027f3b2UL, 0xbdb220e9UL, 0x40217deaUL, 0x40186caaUL, 0x400a9da1UL, 0x3f9b7a78UL, 0x3fa0f31fUL, 0x3fc90fdbUL, 0x00000000UL, 0xc016cbe4UL
};
static const unsigned long hypot_in[] = {
    0x45736014UL, 0x394d6090UL, 0x3e6acef6UL, 0x3d818d89UL, 0x3d2a6117UL, 0x44095a2eUL, 0x420216c9UL, 0x44ff5db9UL, 0x438c47adUL, 0x37d06013UL, 0x468c6c79UL, 0x3db6cd13UL, 0x3c572480UL, 0x432e67b7UL, 0x45e4e266UL, 0x43bf51e5UL, 0x477ebd54UL, 0x400f94d0UL, 0x47822811UL, 0x3aa720d8UL, 0x40400000UL, 0x40800000UL
};
static const unsigned long hypot_want[] = {
    0x45736014UL, 0x3e73945dUL, 0x44095a2eUL, 0x44ff6602UL, 0x438c47adUL, 0x468c6c79UL, 0x432e67b7UL, 0x45e5324eUL, 0x477ebd54UL, 0x47822811UL, 0x40a00000UL
};

#define N(a) ((int) (sizeof (a) / sizeof *(a)))

int main(void) {
    int bad = 0, r = 0;

    bad += check1(exp, exp_in, exp_want, N(exp_want), 2);
    bad += check1(exp2, exp2_in, exp2_want, N(exp2_want), 2);
    bad += check1(expm1, expm1_in, expm1_want, N(expm1_want), 4);
    bad += check1(log, log_in, log_want, N(log_want), 3);
    bad += check1(log2, log2_in, log2_want, N(log2_want), 3);
    bad += check1(log10, log10_in, log10_want, N(log10_want), 3);
    bad += check1(log1p, log1p_in, log1p_want, N(log1p_want), 4);
    bad += check1(sin, sin_in, sin_want, N(sin_want), 4);
    bad += check1(cos, cos_in, cos_want, N(cos_want), 12);
    bad += check1(tan, tan_in, tan_want, N(tan_want), 6);
    bad += check1(atan, atan_in, atan_want, N(atan_want), 3);
    bad += check1(asin, asin_in, asin_want, N(asin_want), 3);
    bad += check1(acos, acos_in, acos_want, N(acos_want), 3);
    bad += check1(sinh, sinh_in, sinh_want, N(sinh_want), 4);
    bad += check1(cosh, cosh_in, cosh_want, N(cosh_want), 3);
    bad += check1(tanh, tanh_in, tanh_want, N(tanh_want), 5);
    bad += check1(cbrt, cbrt_in, cbrt_want, N(cbrt_want), 2);
    bad += check1(sqrt, sqrt_in, sqrt_want, N(sqrt_want), 0);
    bad += check2(pow, pow_in, pow_want, N(pow_want), 16);
    bad += check2(atan2, atan2_in, atan2_want, N(atan2_want), 4);
    bad += check2(hypot, hypot_in, hypot_want, N(hypot_want), 2);
    if (bad == 0) r++;

    /* The ones whose answer is exact, which no tolerance should cover. */
    if (ub(pow(2.0f, 10.0f)) == ub(1024.0f) && ub(pow(9.0f, 0.5f)) == ub(3.0f)) r++;
    if (ub(pow(-2.0f, 3.0f)) == ub(-8.0f) && ub(pow(-2.0f, 2.0f)) == ub(4.0f)) r++;
    if (ub(pow(5.0f, 0.0f)) == ub(1.0f) && ub(pow(1.0f, 99.0f)) == ub(1.0f)) r++;
    if (ub(exp(0.0f)) == ub(1.0f) && ub(log(1.0f)) == ub(0.0f)) r++;
    if (ub(sin(0.0f)) == ub(0.0f) && ub(cos(0.0f)) == ub(1.0f)) r++;
    if (ub(cbrt(-27.0f)) == ub(-3.0f) && ub(cbrt(8.0f)) == ub(2.0f)) r++;
    if (ub(hypot(3.0f, 4.0f)) == ub(5.0f) && ub(atan2(0.0f, 1.0f)) == ub(0.0f)) r++;
    if (ub(sqrt(144.0f)) == ub(12.0f) && ub(log2(1024.0f)) == ub(10.0f)) r++;

    return r + 33 + bad;        /* 9 checks */
}
