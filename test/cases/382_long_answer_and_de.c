/* A call answering a long, in E:UHL, with a byte kept in D across it: DE
 * pushed round the call comes back with D as it was and E the answer's,
 * not the E pushed. A _Bool made before the call and read after it. */
static const unsigned values[] = { 0xffffffu, 0x800000u, 0x7fffffu, 0xabcdefu,
                                   0x123456u, 0x00ff00u, 0x0000ffu, 0u, 0x008040u };

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

        if (v & 0x8000)
            h = mix(h, 1);
        while (!(v & 1) && v)
            v >>= 1;
        h = mix(h, b);
    }
    return h == 1466555645UL ? 42 : 1;
}
