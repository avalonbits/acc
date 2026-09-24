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
static const unsigned long asinh_in[] = {
    0xf0168f68UL, 0xf1324d60UL, 0xf094e40cUL, 0xefeaaf3eUL, 0x71026f35UL, 0x701053f1UL, 0xbf0388c7UL, 0xbfc31f94UL, 0xbf8dddb3UL, 0x3f8322ecUL, 0xbea78d21UL, 0xbffb0a7cUL, 0x3ff7172fUL, 0x3ee84a40UL, 0x40338701UL, 0x3e5f8c8dUL, 0x00000000UL, 0x3727c5acUL, 0x3f000000UL, 0xc0000000UL
};
static const unsigned long asinh_want[] = {
    0xc2882e78UL, 0xc28b4ad7UL, 0xc2898ba6UL, 0xc287aed8UL, 0x428aaac8UL, 0x428818d3UL, 0xbefcaf6dUL, 0xbf9aa70bUL, 0xbf74b852UL, 0x3f660a46UL, 0xbea4b23fUL, 0xbfb68cddUL, 0x3fb4be9aUL, 0x3ee0fad5UL, 0x3fe0a222UL, 0x3e5dcf75UL, 0x00000000UL, 0x3727c5acUL, 0x3ef66165UL, 0xbfb8c90cUL
};
static const unsigned long acosh_in[] = {
    0x5e88947cUL, 0x54c48395UL, 0x5dc4c00dUL, 0x6d3de569UL, 0x705dd22bUL, 0x6069d378UL, 0x54eb0893UL, 0x4297dcb5UL, 0x402b8ba1UL, 0x3fe43433UL, 0x3fe1e818UL, 0x3f8e9efeUL, 0x3f88bca5UL, 0x3fe3fa31UL, 0x403cc3f0UL, 0x401ecb30UL, 0x3f800000UL, 0x3f800347UL, 0x40000000UL
};
static const unsigned long acosh_want[] = {
    0x422eeec0UL, 0x41f1df4cUL, 0x422ad8f8UL, 0x428053f6UL, 0x4288f4dfUL, 0x423966abUL, 0x41f34de1UL, 0x40a0bbd7UL, 0x3fd23856UL, 0x3f97370cUL, 0x3f95a5a3UL, 0x3ef272cdUL, 0x3ebc1ce5UL, 0x3f970fb7UL, 0x3fdf528cUL, 0x3fc77e1bUL, 0x00000000UL, 0x3c67b8d8UL, 0x3fa89214UL
};
static const unsigned long atanh_in[] = {
    0x3ed59a31UL, 0x3eac50fdUL, 0xbee85866UL, 0xbf297ac4UL, 0x3e516904UL, 0xbf5f3baaUL, 0x3e34334dUL, 0x3cecfec0UL, 0x3dade627UL, 0xbf41aec4UL, 0xbf5d2aecUL, 0xbd5b0f3bUL, 0xbf3548bfUL, 0x3e90b46eUL, 0x00000000UL, 0x3727c5acUL, 0x3f000000UL, 0xbf7ff972UL
};
static const unsigned long atanh_want[] = {
    0x3ee379a6UL, 0x3eb34dc8UL, 0xbefa9c6eUL, 0xbf4be20eUL, 0x3e5467adUL, 0xbfabb275UL, 0x3e36188cUL, 0x3ced0fafUL, 0x3dae519cUL, 0xbf7cf77dUL, 0xbfa78233UL, 0xbd5b44caUL, 0xbf62297eUL, 0x3e94c0ecUL, 0x00000000UL, 0x3727c5acUL, 0x3f0c9f54UL, 0xc09e73cdUL
};
static const unsigned long erf_in[] = {
    0xbfc30900UL, 0x3f77144fUL, 0x40001b93UL, 0x3f59fb6bUL, 0x3e815d67UL, 0xc01af8afUL, 0xbf9c7a76UL, 0x4074b59bUL, 0xbfecfc31UL, 0xbfdcd911UL, 0xbd5c1161UL, 0x3ebc3a32UL, 0x4014dc73UL, 0xbfd1abc8UL, 0x406cf7c4UL, 0xc0749eb2UL, 0x4014aa66UL, 0x3f0ece0eUL, 0x404d656cUL, 0xc031a16bUL, 0x00000000UL, 0x3727c5acUL, 0x3f000000UL, 0x3f800000UL, 0xbfc00000UL, 0x40400000UL
};
static const unsigned long erf_want[] = {
    0xbf780508UL, 0x3f53e5daUL, 0x3f7ecfb6UL, 0x3f458006UL, 0x3e8eecc7UL, 0xbf7fd79fUL, 0xbf6a89d2UL, 0x3f7fffffUL, 0xbf7dbcefUL, 0xbf7c3d95UL, 0xbd7814cfUL, 0x3ecb32faUL, 0x3f7fbe33UL, 0xbf7abeb2UL, 0x3f7ffffdUL, 0xbf7fffffUL, 0x3f7fbd2fUL, 0x3f11e015UL, 0x3f7fffa1UL, 0xbf7ffa52UL, 0x00000000UL, 0x373d4f84UL, 0x3f053f7bUL, 0x3f57bb3dUL, 0xbf7752abUL, 0x3f7ffe8dUL
};
static const unsigned long erfc_in[] = {
    0x40a8a915UL, 0x4105dd68UL, 0x40a02643UL, 0xbfd35f9eUL, 0x4100d1d1UL, 0x410a2b9dUL, 0xbfd5ce15UL, 0x40ddabc5UL, 0xc0031359UL, 0xc0019139UL, 0x40d16c82UL, 0x40b472a1UL, 0x4107c7aaUL, 0x4092a924UL, 0x4048fbedUL, 0x3e8796c8UL, 0x4077f3f7UL, 0x410e8d4dUL, 0x409851d0UL, 0x40f03592UL, 0xbf21706fUL, 0x40ba3a2cUL, 0x40b90089UL, 0xc009956aUL, 0x4108fc4cUL, 0x405a25ffUL, 0x406f4774UL, 0x401f573cUL, 0x40e9a4c1UL, 0x4048db39UL, 0x40000559UL, 0x41079d61UL, 0x41012c67UL, 0x40d1bda4UL, 0x40d26d53UL, 0x41062427UL, 0x40e2e8a4UL, 0x40475babUL, 0x4105ff10UL, 0x409a97c6UL, 0x40d571bdUL, 0x40c32eebUL, 0x40aa138bUL, 0x410d3cdcUL, 0x4029e27bUL, 0x40937fcbUL, 0x00000000UL, 0x3f000000UL, 0x3f800000UL, 0x40000000UL, 0x40800000UL, 0x41100000UL
};
static const unsigned long erfc_want[] = {
    0x29cc32c6UL, 0x0b0a53f4UL, 0x2bce5070UL, 0x3ffd803cUL, 0x0ec6d5c4UL, 0x07b0d6e9UL, 0x3ffdacc3UL, 0x1b0cd070UL, 0x3fff844eUL, 0x3fff7684UL, 0x1ec9bd83UL, 0x26dc1109UL, 0x09912bddUL, 0x2ec7a3e3UL, 0x37161cd2UL, 0x3f3540deUL, 0x3337abd2UL, 0x0433c6f2UL, 0x2d939102UL, 0x14f911b6UL, 0x3fd05283UL, 0x25578e7aUL, 0x25a92038UL, 0x3fffb287UL, 0x089f52c2UL, 0x35c04068UL, 0x34053ca7UL, 0x39e16e81UL, 0x1726e9e6UL, 0x3718182dUL, 0x3b990f23UL, 0x09ad295cUL, 0x0e8ad185UL, 0x1eb0eee4UL, 0x1e8515cdUL, 0x0acebba6UL, 0x195dfda8UL, 0x37314911UL, 0x0af0ddfaUL, 0x2d13354dUL, 0x1d969115UL, 0x22ea7a45UL, 0x297d9ba7UL, 0x0542b32aUL, 0x39368532UL, 0x2e9c0d5aUL, 0x3f800000UL, 0x3ef5810aUL, 0x3e21130bUL, 0x3b9947afUL, 0x32846ee9UL, 0x030cc6a1UL
};
static const unsigned long tgamma_in[] = {
    0x41bbc984UL, 0x41ab6062UL, 0x41e91012UL, 0x40e91cfaUL, 0x41e13082UL, 0x41deca40UL, 0x40a059beUL, 0x409ce25cUL, 0x41a35a94UL, 0x42089692UL, 0x41abe02cUL, 0x416fbed2UL, 0x418df646UL, 0x41e2b07bUL, 0x4192474dUL, 0x41965461UL, 0x41a626f5UL, 0x42027efcUL, 0x41e02099UL, 0x41eaf96dUL, 0xc0eae64cUL, 0xc0802503UL, 0xbfe380e2UL, 0xc034d6dfUL, 0xbefb92d9UL, 0xbf5b670fUL, 0xc10b9991UL, 0xc09d89c3UL, 0xc0cda17bUL, 0xc0fe6c0aUL, 0x3f000000UL, 0x3f800000UL, 0x40000000UL, 0x40600000UL, 0x41200000UL, 0x420a0000UL
};
static const unsigned long tgamma_want[] = {
    0x6385b0afUL, 0x5ef2aa61UL, 0x70c03abeUL, 0x449a5c5bUL, 0x6e6674d7UL, 0x6daa9382UL, 0x41c3325dUL, 0x41a5fc71UL, 0x5cbc83d2UL, 0x7d2f394bUL, 0x5f12b292UL, 0x519b9e3aUL, 0x571c508aUL, 0x6ed6d959UL, 0x58372c5bUL, 0x59461ffaUL, 0x5d869e6aUL, 0x795b7de1UL, 0x6e145c03UL, 0x7156744bUL, 0x39ba14fcUL, 0xc1128f90UL, 0x403dd9e0UL, 0xbf9f7c42UL, 0xc063057eUL, 0xc0f44ea9UL, 0xb7b0b0edUL, 0xbdff4b45UL, 0xbb028cf2UL, 0x3a13179eUL, 0x3fe2dfc5UL, 0x3f800000UL, 0x3f800000UL, 0x4054b1c8UL, 0x48b13000UL, 0x7e17ce58UL
};
static const unsigned long lgamma_in[] = {
    0x40805d53UL, 0x40fbce8aUL, 0x40c4c07fUL, 0x3f4d5461UL, 0x3ffea375UL, 0x403dab5bUL, 0x40e2a291UL, 0x406b9bcaUL, 0x3fa46d00UL, 0x408cfe7cUL, 0x3ffe1f8bUL, 0x3ea1f54bUL, 0x4097da70UL, 0x40fda9dcUL, 0x667fb70bUL, 0x648f00f5UL, 0x4857c7efUL, 0x5c1cebdbUL, 0x5fbd36a1UL, 0x65a61061UL, 0x5690caf0UL, 0x438e4772UL, 0xbf3b719bUL, 0xc0eb1cc9UL, 0xc027e226UL, 0xc0ab99dfUL, 0xc00dd04fUL, 0xc0d408f3UL, 0x3f000000UL, 0x3f8020c5UL, 0x3fffdf3bUL, 0x40200000UL, 0x40400000UL, 0x42c80000UL
};
static const unsigned long lgamma_want[] = {
    0x3fe72de2UL, 0x410431eaUL, 0x40a15edaUL, 0x3e19ab7bUL, 0xbb92290cUL, 0x3f28e90aUL, 0x40d77ccaUL, 0x3fb404d9UL, 0xbdd7eb7dUL, 0x4014c2a0UL, 0xbbc8d9dfUL, 0x3f852546UL, 0x403352ceUL, 0x41060db3UL, 0x6954056cUL, 0x67613f60UL, 0x4a1878edUL, 0x5ebdd74aUL, 0x62815a0fUL, 0x6886c524UL, 0x590c4deaUL, 0x44a531cbUL, 0x3fc377b0UL, 0xc0fefdb0UL, 0xbdf1367fUL, 0xc085a712UL, 0x3f35b4f1UL, 0xc0d1c7baUL, 0x3f128682UL, 0xba171aedUL, 0xb9dd8083UL, 0x3e91c1f4UL, 0x3f317218UL, 0x43b3912eUL
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
    bad += check1(asinh, asinh_in, asinh_want, N(asinh_want), 2);
    bad += check1(acosh, acosh_in, acosh_want, N(acosh_want), 2);
    bad += check1(atanh, atanh_in, atanh_want, N(atanh_want), 3);
    bad += check1(erf, erf_in, erf_want, N(erf_want), 2);
    bad += check1(erfc, erfc_in, erfc_want, N(erfc_want), 4);
    bad += check1(tgamma, tgamma_in, tgamma_want, N(tgamma_want), 10);
    bad += check1(lgamma, lgamma_in, lgamma_want, N(lgamma_want), 10);
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
