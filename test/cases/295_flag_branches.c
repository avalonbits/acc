/* A branch, a `!` and a conversion to _Bool straight after an AND that keeps
 * one byte, or after a comparison: each reads the flags those left rather
 * than testing the value against zero -- and must come to what the test
 * would have. Values with bits in all three bytes, signed and not. */
static unsigned values[] = { 0xffffffu, 0x800000u, 0x7fffffu, 0xabcdefu,
                             0x123456u, 0x00ff00u, 0x0000ffu, 0u, 0x008040u };
static int signs[] = { -1, 0, 1, -8388608, 8388607, -256, 255 };

static unsigned long mix(unsigned long h, int x)
{
    return (h * 31 + (unsigned) x) & 0xffffffffUL;
}

int main(void)
{
    unsigned long h = 0;

    for (int i = 0; i < 9; i++) {
        unsigned v = values[i];
        _Bool b = v & 0x80;

        if (v & 0x8000) h = mix(h, 1);
        if (!(v & 0x40)) h = mix(h, 2);
        if (v & 0xff00) h = mix(h, 3);
        if (v & 0x00ff) h = mix(h, 4);
        if (v & 0x0180) h = mix(h, 8);          /* two bytes: still tested */
        h = mix(h, !(v & 0x8001));
        while (!(v & 1) && v) v >>= 1;
        h = mix(h, (int) v);
        h = mix(h, !(v & 0x0100));
        h = mix(h, !!(v & 0x0400));
        h = mix(h, b);
        h = mix(h, (v & 0x10) ? 5 : 6);
        h = mix(h, (v & 0x10) && (v & 0x2000));
        h = mix(h, (v & 0x10) || (v & 0x2000));
        h = mix(h, (int) (v & 0x2000));
    }
    for (int i = 0; i < 7; i++)
        for (int j = 0; j < 7; j++) {
            int a = signs[i], c = signs[j];
            unsigned ua = a, uc = c;

            h = mix(h, !(a < c));
            h = mix(h, !(a >= c));
            h = mix(h, !(a == c));
            h = mix(h, !(a != c));
            h = mix(h, !(ua < uc));
            h = mix(h, !!(ua > uc));
            h = mix(h, (_Bool) (a <= c));
            if (!(a > c)) h = mix(h, 7);
        }

    /* From the same program built by gcc on the host, whose answer does
     * not depend on the width of int: every value fits in 24 bits. */
    return h == 3956180264UL ? 42 : 1;
}
