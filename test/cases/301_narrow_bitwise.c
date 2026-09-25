/* &, | and ^ of values known to fit in a byte or two -- unsigned chars and
 * shorts as C promotes them, and what an AND with one of them leaves -- are
 * done a byte or two at a time rather than called for. What they come to
 * has to be what the whole ints would: against signed chars, whose sign
 * fills the upper bytes; against wide and negative ints; through ?: with
 * one narrow side and one not; spilled and read back; and tested against
 * zero. And a register variable, whose own qualifier is a bit that once
 * said a value was narrow. */
static const unsigned char ub[] = { 0x00, 0x01, 0x7f, 0x80, 0xa5, 0xff };
static const signed char sb[] = { 0, 1, 127, -128, -91, -1 };
static const unsigned short uw[] = { 0x0000, 0x00ff, 0x0100, 0x8000, 0xa55a, 0xffff };
static const int wide[] = { 0, 1, 0x1234, 0x123456, -1, 0x10000 };

static unsigned char byte_of(int i) { return ub[i]; }
static unsigned short word_of(int i) { return uw[i]; }

static unsigned long mix(unsigned long h, long x)
{
    return (h * 31 + (unsigned long) x) & 0xffffffffUL;
}

int main(void)
{
    unsigned long h = 0;
    int i, j;

    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++) {
            unsigned char a = ub[i], b = ub[j];
            signed char c = sb[j];
            unsigned short w = uw[j];
            int x = wide[j], t;
            register int rx = wide[i];

            h = mix(h, a & b);
            h = mix(h, a | b);
            h = mix(h, a ^ b);
            h = mix(h, a & c);
            h = mix(h, a | c);
            h = mix(h, a ^ c);
            h = mix(h, a & w);
            h = mix(h, a | w);
            h = mix(h, w ^ a);
            h = mix(h, w & uw[i]);
            h = mix(h, w | uw[i]);
            h = mix(h, a & x);
            h = mix(h, x & w);
            h = mix(h, a | x);
            h = mix(h, a ^ x);
            h = mix(h, (a & x) | (b & w));
            h = mix(h, (unsigned char) (a ^ b) ^ (unsigned char) ~a);
            h = mix(h, ((i & 1) ? a : x) & b);
            h = mix(h, ((i & 1) ? a : x) | b);
            h = mix(h, ((i & 1) ? a : c) ^ b);
            h = mix(h, ((j & 1) ? a : x) & wide[i]);
            h = mix(h, ((j & 1) ? a : x) != 0);
            if ((j & 1) ? b : x) h = mix(h, 3);
            h = mix(h, ((j & 1) ? (a & b) : x) & wide[i]);
            h = mix(h, ((j & 1) ? x : (a | b)) & wide[i]);
            h = mix(h, ((j & 1) ? (a ^ b) : x) != 0);
            h = mix(h, byte_of(i) ^ byte_of(j));
            h = mix(h, word_of(i) & byte_of(j));
            h = mix(h, rx & x);
            h = mix(h, (rx | a) != 0);
            h = mix(h, (signed char) (a & b));
            h = mix(h, (short) (w | a));
            h = mix(h, (unsigned short) (a ^ b));
            h = mix(h, (a & b) != 0);
            h = mix(h, (w & a) == 0);
            h = mix(h, (unsigned char) (a | c) != 0);
            if (a & b) h = mix(h, 1);
            if (!(w & uw[i])) h = mix(h, 2);
            t = (a ^ b) + (a & 0x0f) * (b | 3) + ((a | w) ^ (x & b));
            h = mix(h, t);
            t = x;
            t &= a;
            t |= b;
            t ^= w;
            h = mix(h, t);
        }

    /* From gcc on the host: every value fits in 24 bits, or is negative the
     * same way in both. */
    return h == 1823022505UL ? 42 : 1;
}
