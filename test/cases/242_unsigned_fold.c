/* Unsigned values where signed and unsigned disagree.
 *
 * acc keeps every int sign-extended on its value stack, which is the signed
 * reading of the bits, so 0xffffffU sits there as -1. Adding and subtracting
 * do not care. A right shift, a division and a remainder do, and the folder
 * did all three on the signed reading: 0x800000U / 2U came out as -0x400000
 * and -1U >> 9 as -1, the shift a signed value gets. Found by gcc's
 * pr37924, which compares against -1U >> 9, and by three more tests that
 * fold an unsigned constant the same way.
 *
 * The same answer came out at run time for a different reason: -x on an
 * unsigned was pushed as a plain int, so `-u >> 9` shifted the sign in too.
 * C99 6.5.3.3 gives -x the promoted type of x. ~x is -x - 1 here, so it
 * went wrong the same way.
 */
static unsigned int id(unsigned int v) { return v; }

int main(void) {
    int r = 0;
    unsigned int m = id(1U), z = id(0U);

    /* Folded: every constant here is known. */
    if ((-1U >> 9) == 0x7fffU && (~0U >> 9) == 0x7fffU) r++;
    if (((0U - 1U) >> 9) == 0x7fffU && ((unsigned) -1 >> 9) == 0x7fffU) r++;
    if (0x800000U / 2U == 0x400000U && 0xffffffU % 10U == 5U) r++;
    if ((0x800000U >> 4) == 0x80000U) r++;

    /* At run time: the negation has to stay unsigned. */
    if ((-m >> 9) == 0x7fffU && ((-m) >> 1) == 0x7fffffU) r++;
    if ((~z >> 9) == 0x7fffU) r++;
    if ((-m) / 2U == 0x7fffffU && (-m) % 10U == 5U) r++;

    /* And a signed one is still signed. */
    {
        int n = (int) id(1U);

        if ((-n >> 9) == -1 && -n / 2 == 0) r++;
    }

    return r + 34;              /* 8 checks */
}
