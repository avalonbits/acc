/* `!`, `&&` and `||`, and what it means for a value to be true.
 *
 * A value is true when it compares unequal to zero at its own type, which
 * is not the same thing as its bytes being non-zero. A long of 0x1000000 is
 * true though its low three bytes are all zero; a float is false at -0.0 as
 * well as 0.0, and 0.5 is true although it would convert to the integer 0.
 * `if` got both of those wrong before this -- it tested the low three bytes of
 * whatever it was given -- and `!`, `&&` and `||` now share the test with it.
 *
 * `&&` and `||` stop as soon as the answer is known, and C guarantees that
 * the right side is then never evaluated. bump() counts how often it is
 * reached, which is how the short circuit is seen rather than assumed.
 */
int bump(int *count) {
    *count = *count + 1;

    return 1;
}

int main(void) {
    int zero = 0;
    int five = 5;
    long high = 16777216;           /* 0x1000000: only the fourth byte set */
    float half = 0.5;
    float negzero = -0.0;
    int *p = &five;
    int calls = 0;
    unsigned char big = 255;
    unsigned char small;
    int r = 0;

    if (!zero == 1) r = r + 1;
    if (!five == 0) r = r + 1;
    if ((five && five) == 1) r = r + 1;
    if ((five && zero) == 0) r = r + 1;
    if ((zero || five) == 1) r = r + 1;
    if ((zero || zero) == 0) r = r + 1;

    /* Truth at the operand's own type. */
    if (high) r = r + 1;
    if (!high == 0) r = r + 1;
    if (half) r = r + 1;
    if (negzero) r = r + 1000;
    if (!negzero == 1) r = r + 1;
    if (p && 1) r = r + 1;

    /* The short circuit: the right side is reached twice out of four. */
    if ((zero && bump(&calls)) == 0) r = r + 1;
    if ((five || bump(&calls)) == 1) r = r + 1;
    if (zero || bump(&calls)) r = r + 1;
    if (five && bump(&calls)) r = r + 1;
    if (calls == 2) r = r + 1;

    /* The right side of || going into a char is still compared at int
     * width: a byte of 255 plus 1 is 256 once C has promoted it, which is
     * true -- and it would be 0, and false, if the add were done in eight
     * bits because of where the answer is going. */
    small = zero || big + 1;
    if (small == 1) r = r + 1;

    /* && binds tighter than ||, and both looser than a comparison. */
    if ((1 || 0 && 0) == 1) r = r + 1;
    if (five > 1 && zero == 0) r = r + 1;

    /* 19 */
    return r + 23;
}
