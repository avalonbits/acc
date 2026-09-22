/* An unsigned constant on its way to a wider type.
 *
 * A constant is kept on the stack the way the machine would hold it: with
 * its top bit run up through the host's word, so that `0x800000` has the
 * bits a 24-bit int of that value has and reads, as a number, as negative.
 * That is right for a signed type and has to stop at the type's own width
 * for an unsigned one -- and it did not. `unsigned long x = 0x800000u` came
 * out 0xff800000, and every unsigned constant with the top bit of an int
 * set widened wrong wherever it was used.
 *
 * It took a while to find because nothing in C written by hand does this
 * often. acc's own float.c does: `m | 0x800000u` puts back the bit an IEEE
 * significand leaves out, so a compiler with this bug cannot compile the
 * compiler -- every float it folded came out wrong, and only then.
 */
unsigned long g_bit = 0x800000u;
unsigned long g_all = 0xffffffu;
unsigned long g_low = 0x7fffffu;

static unsigned long take(unsigned long v) { return v; }
static unsigned long long take64(unsigned long long v) { return v; }

int main(void) {
    int r = 0;
    unsigned long a = 0x800000u;
    unsigned long b;
    unsigned long long w = 0x800000u;

    b = 0xffffffu;

    if (a == 0x800000UL && b == 0xffffffUL) r++;
    if (g_bit == 0x800000UL && g_all == 0xffffffUL && g_low == 0x7fffffUL) r++;
    if (take(0x800000u) == 0x800000UL) r++;
    if (take(0xffffffu) == 0xffffffUL) r++;
    if ((unsigned long) 0x800000u == 0x800000UL) r++;
    if (0x800000u == 0x800000UL && 0xffffffu == 0xffffffUL) r++;

    /* And on to eight bytes, where the bits above have further to go. */
    if (w == 0x800000ULL && take64(0xffffffu) == 0xffffffULL) r++;
    if ((unsigned long long) 0x800000u == 0x800000ULL) r++;

    /* A signed one still widens by its sign, which is what makes the two
     * different: the same bits, read two ways. */
    if ((long) (int) 0x800000u == -8388608L && (long) -1 == -1L) r++;

    /* The shift float.c does with it: the significand's top bit put back,
     * then moved up to where the exponent says. */
    {
        unsigned long m = 0x5e0b6bu | 0x800000u;

        if (m == 0xde0b6bUL && ((unsigned long long) m << 36)
            == 999999984306749440ULL) r++;
    }

    return r + 32;              /* 10 checks */
}
