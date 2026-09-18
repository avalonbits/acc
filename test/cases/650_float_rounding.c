/* Float literals on and around the midpoint between two floats.
 *
 * A literal is rounded to the nearest float, and one exactly halfway goes to
 * the neighbour whose last bit is zero. strtod got these wrong on the Agon,
 * where a double is 32 bits and it does not round correctly, and could get
 * them wrong on the host too, rounding once to a double and again to a float.
 * Each literal here is compared with one that is unambiguous about which float
 * it means, so a conversion that lands on the wrong side shows.
 */
int main(void) {
    int r = 0;
    float huge = 3.40282356779733661637539395458142568448e38;

    /* 2^24 + 1 is halfway between 2^24 and 2^24 + 2, and goes down to the
     * even one; 2^24 + 3 goes up. Just past a midpoint goes past it. */
    if (16777217.0 == 16777216.0) r = r + 1;
    if (16777219.0 == 16777220.0) r = r + 1;
    if (16777217.0000000000000000000000000000001 == 16777218.0) r = r + 1;

    /* 1 + 2^-24, halfway between 1 and the next float: down to 1. A digit
     * past it is closer to 1 + 2^-23. */
    if (1.000000059604644775390625 == 1.0) r = r + 1;
    if (1.000000059604644775390625000001 == 1.00000011920928955078125) r = r + 1;
    if (1.00000011920928955 == 1.00000011920928955078125) r = r + 1;

    /* 2^-150, half the smallest subnormal, goes down to zero, and anything
     * past it up to the smallest subnormal. */
    if (7.00649232162408535461864791644958065640130970938257885878534141944895541342930300743319094181060791015625e-46 == 0.0) r = r + 1;
    if (7.00649232162408535461864791644958065640130970938257885878534141944895541342930300743319094181060791015625001e-46 == 1.40129846e-45) r = r + 1;

    /* Halfway between the largest float and 2^128 rounds to the even one,
     * 2^128, which is infinity: the only float that doubling leaves alone. */
    if (huge == huge * 2.0) r = r + 1;
    if (3.4028235e38 != 3.4028235e38 * 2.0) r = r + 1;

    /* 10 */
    return r + 32;
}
