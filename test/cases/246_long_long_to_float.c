/* A long long constant converted to a float.
 *
 * Folded at compile time, the conversion took the value's magnitude as 32
 * bits, so anything past 2^32 arrived with its top half gone: the unsigned
 * long long 2^64 - 1 became the float of 2^32 - 1. The same conversions done
 * at run time go through the runtime's routine and were right -- though
 * not in a case compared with agondev, whose library has no routine for an
 * unsigned long long at all. Found by
 * gcc's 920710-1, which checks that (double) of the largest unsigned long
 * long lands near 1.8e19.
 *
 * Compared as bits, because the value is what is being tested and a float
 * comparison would convert both sides the same wrong way.
 */
static unsigned long bits(float f) {
    union { float f; unsigned long u; } v;

    v.f = f;

    return v.u;
}

int main(void) {
    int r = 0;

    /* Folded: past 2^32, where the top half used to go. */
    if (bits((float) 18446744073709551615ULL) == 0x5f800000UL) r++;  /* 2^64 */
    if (bits((float) 9223372036854775808ULL) == 0x5f000000UL) r++;   /* 2^63 */
    if (bits((float) 1099511627776LL) == 0x53800000UL) r++;          /* 2^40 */
    if (bits((float) -1099511627776LL) == 0xd3800000UL) r++;         /* -2^40 */

    /* The most negative long long, whose magnitude does not fit in one. */
    if (bits((float) (-9223372036854775807LL - 1)) == 0xdf000000UL) r++;

    /* Below 2^32 it was always right, and still is. */
    if (bits((float) 16777216LL) == 0x4b800000UL && (float) -5LL == -5.0f) r++;

    /* Rounded, not truncated: 2^40 + 2^16 + 1 is nearest to 2^40 + 2^17. */
    if (bits((float) 1099511693313LL) == 0x53800001UL) r++;

    /* The check the gcc test makes. */
    if (!((float) 18446744073709551615ULL < 1.84467e19f
          || (float) 18446744073709551615ULL > 1.84468e19f)) r++;

    return r + 34;              /* 8 checks */
}
