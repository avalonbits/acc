/* Dividing, at the edges the routine has.
 *
 * The divisor arrives in BC, which is twenty-four bits wide, and the check
 * for dividing by nothing used to be `ld a, b` and `or a, c` -- which reads
 * sixteen of them. Every divisor that is a multiple of 65536 looked like
 * zero, so `x / 65536` answered nothing at all, for every x. So the divisors
 * here include 65536 and its neighbours on both sides.
 *
 * The rest is the loop itself: twenty-four rounds of shifting the remainder
 * up and taking the divisor off it when it fits. The round where the shift
 * carries out of twenty-four bits is the one no small case reaches, so the
 * dividends run up to the top of the range as well.
 *
 * Signed division on top of that has to truncate towards zero and leave the
 * remainder with the sign of the dividend, which is what C99 says and is not
 * what flooring gives: -7 / 2 is -3 and -7 % 2 is -1.
 *
 * Dividing by nothing is not here. It is undefined, so acc answering nothing
 * and agondev answering something else are both allowed, and a case that
 * compares them is testing which way two compilers happened to jump.
 */
static int op(int x) { return x; }
static unsigned uop(unsigned x) { return x; }
static long lop(long x) { return x; }

int main(void)
{
    /* The divisor whose low sixteen bits are nothing. */
    if (uop(131072u) / uop(65536u) != 2u) return 1;
    if (uop(131072u) % uop(65536u) != 0u) return 2;
    if (uop(200000u) / uop(65536u) != 3u) return 3;
    if (uop(200000u) % uop(65536u) != 3392u) return 4;
    if (uop(65535u) / uop(65536u) != 0u) return 5;
    if (uop(16777215u) / uop(65536u) != 255u) return 6;
    if (uop(1000000u) / uop(131072u) != 7u) return 7;
    if (uop(1000000u) / uop(262144u) != 3u) return 8;

    /* Either side of it, so a wrong answer cannot be a coincidence. */
    if (uop(131072u) / uop(65535u) != 2u) return 9;
    if (uop(131072u) / uop(65537u) != 1u) return 10;

    /* Ordinary ones. */
    if (uop(100u) / uop(7u) != 14u) return 11;
    if (uop(100u) % uop(7u) != 2u) return 12;
    if (uop(1u) / uop(1u) != 1u) return 13;
    if (uop(0u) / uop(5u) != 0u) return 14;

    /* The top of the range, where the remainder's shift carries out. */
    if (uop(16777215u) / uop(3u) != 5592405u) return 15;
    if (uop(16777215u) % uop(3u) != 0u) return 16;
    if (uop(16777215u) / uop(16777215u) != 1u) return 17;
    if (uop(16777214u) / uop(16777215u) != 0u) return 18;
    if (uop(16777215u) % uop(16777214u) != 1u) return 19;
    if (uop(8388608u) / uop(2u) != 4194304u) return 20;

    /* Signed, truncating towards zero with the remainder following the
     * dividend. */
    if (op(-7) / op(2) != -3) return 21;
    if (op(-7) % op(2) != -1) return 22;
    if (op(7) / op(-2) != -3) return 23;
    if (op(7) % op(-2) != 1) return 24;
    if (op(-7) / op(-2) != 3) return 25;
    if (op(-7) % op(-2) != -1) return 26;
    if (op(-8388608) / op(1) != -8388608) return 27;
    if (op(-131072) / op(65536) != -2) return 28;

    /* Longs go through a routine of their own. */
    if (lop(1000000L) / lop(65536L) != 15L) return 30;
    if (lop(-1000000L) / lop(65536L) != -15L) return 31;

    return 42;
}
