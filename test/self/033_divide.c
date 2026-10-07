/* Integer division and remainder, unsigned and signed, against a
 * reference made of shifts, subtractions and compares alone: dividends of
 * every width, the top byte or two of them zero -- which the runtime's
 * divide takes eight rounds at a time -- and divisors from 1 up. */
static unsigned ref_q, ref_r;

static void reference(unsigned n, unsigned d)
{
    unsigned q = 0, r = 0;
    int bit;

    for (bit = 23; bit >= 0; bit--) {
        r = (r << 1) | ((n >> bit) & 1);
        q <<= 1;
        if (r >= d) {
            r -= d;
            q |= 1;
        }
    }
    ref_q = q & 0xffffff;
    ref_r = r & 0xffffff;
}

static unsigned next_seed = 1;

static unsigned rnd(void)
{
    next_seed = next_seed * 1103515245u + 12345u;

    return next_seed;
}

int main(void)
{
    static const unsigned widths[] = { 0xff, 0xffff, 0xffffff, 0x7fffff };
    int i, w, bad = 0;

    for (w = 0; w < 4; w++)
        for (i = 0; i < 300; i++) {
            unsigned n = rnd() & widths[w];
            unsigned d = (rnd() & widths[(i % 4)]) | 1;
            volatile unsigned vn = n, vd = d;

            if (i % 7 == 0)
                d = (unsigned) (i % 9 + 1), vd = d;
            reference(n, d);
            if (vn / vd != ref_q || vn % vd != ref_r)
                bad++;
            /* Signed: both magnitudes, the quotient toward zero, the
             * remainder with the dividend's sign. */
            if (n < 0x800000 && d < 0x800000) {
                volatile int sn = -(int) n, sd = (int) d;

                if (sn / sd != -(int) ref_q || sn % sd != -(int) ref_r)
                    bad++;
                sd = -(int) d;
                if ((int) vn / sd != -(int) ref_q || (int) vn % sd != (int) ref_r)
                    bad++;
            }
        }

    return bad ? (bad > 40 ? 40 : bad) : 42;
}
