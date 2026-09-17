/* Adding and subtracting floats.
 *
 * The exact sums are the easy half. What the eight spare bits below the
 * significand are for is the other half: aligning the smaller operand shifts
 * bits down into them, and rounding then reads them. 16777217 is the first
 * integer a float cannot hold, so 16777216 + 1 has to round to even and give
 * 16777216 back, while 16777218 + 1 rounds the other way to 16777220.
 *
 * Cancellation is the case that needs the most shifting afterwards: taking
 * two nearly equal numbers apart leaves a significand with almost no leading
 * bits, and the result is only right if it is shifted back up and the
 * exponent brought down with it.
 */
int main(void) {
    float a = 1.5;
    float b = 2.5;
    float big = 16777216.0;         /* 2^24: the first gap in the integers */
    float near = 1.00000011920928955;
    int r = 0;

    if (a + b == 4.0) r = r + 1;
    if (b - a == 1.0) r = r + 1;
    if (a - b == -1.0) r = r + 1;
    if (a + 0.0 == a) r = r + 1;
    if (a - a == 0.0) r = r + 1;

    /* A tie, resolved to the even significand. */
    if (big + 1.0 == big) r = r + 1;
    if (big + 2.0 == 16777218.0) r = r + 1;

    /* The operands the other way round, which is the same sum and takes the
     * other path through the alignment. */
    if (1.0 + big == big) r = r + 1;

    /* And the reason the bits shifted past the bottom are remembered rather
     * than dropped. Aligning `near` under big moves it twenty-four places,
     * which is further than the eight spare bits reach, so everything below
     * them falls off. What falls off is the difference between a sum that is
     * exactly half way -- a tie, which rounds down to the even 16777216 --
     * and one that is a hair above it and has to round up. */
    if (big + near == 16777218.0) r = r + 1;

    /* Cancellation: what is left has to be shifted back to the top. */
    if (near - 1.0 == 0.00000011920928955) r = r + 1;

    /* Signs, where the smaller magnitude is on the left. */
    if (-a + b == 1.0) r = r + 1;
    if (-b + a == -1.0) r = r + 1;

    /* 12 */
    return r + 30;
}
