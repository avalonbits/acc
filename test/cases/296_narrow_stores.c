/* An assignment to a narrow local stores the low bytes and converts after,
 * for the value, only when something reads it: a chain, a condition, a
 * return, a comma's right side. Values past 16 bits and past 8, signed and
 * not, so a conversion left out or left in the wrong place shows. */
static unsigned values[] = { 0x12345u, 0xffffu, 0x10000u, 0x8000u, 0x80u,
                             0x1ffu, 0xabcdefu, 0u, 0x7fffu, 0xff80u };

static unsigned long mix(unsigned long h, long x)
{
    return (h * 33 + (unsigned long) x) & 0xffffffffUL;
}

static unsigned short twice(unsigned x)
{
    unsigned short c;

    return c = x + x;
}

int main(void)
{
    unsigned long h = 0;

    for (int i = 0; i < 10; i++) {
        unsigned x = values[i];
        unsigned short us;
        short s;
        unsigned char uc;
        signed char sc;
        int n;

        us = x;                 h = mix(h, us);
        s = x;                  h = mix(h, s);
        uc = x;                 h = mix(h, uc);
        sc = x;                 h = mix(h, sc);
        n = us = x + 1;         h = mix(h, n);
        n = s = x + 1;          h = mix(h, n);
        n = sc = uc = x;        h = mix(h, n + sc + uc);
        if ((us = x) == 0)      h = mix(h, 1);
        if ((s = x) < 0)        h = mix(h, 2);
        h = mix(h, (us = x << 1, us));
        h = mix(h, (uc = x - 1, uc));
        h = mix(h, twice(x));
        for (us = x; us > 0xff00; us = us + 0x40)
            h = mix(h, us);
        {
            unsigned short init = x * 3;
            signed char low = x;

            h = mix(h, init + low);
        }
        us = 0x8000;
        us += x;                h = mix(h, us);
        s = 0x7fff;
        n = (s += x);           h = mix(h, n);
    }

    /* From gcc on the host: every value fits the machine's int. */
    return h == 242012989UL ? 42 : 1;
}
