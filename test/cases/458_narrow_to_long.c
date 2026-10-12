/* A value known to fit in a byte or two, made a long: its top byte 0
 * whatever its type, with no test of its sign -- an unsigned char read
 * through a pointer and as a local, an unsigned short, an int ANDed with a
 * byte. A signed char, which is not known to fit, still takes its sign. */
static long from_uchar(const unsigned char *p) { long x = *p; return x; }
static long from_ushort(const unsigned short *p) { long x = *p; return x; }
static long from_and(int v) { long x = v & 0xff; return x; }
static long from_schar(const signed char *p) { long x = *p; return x; }
static long sum(const unsigned char *p, int n)
{
    long s = 0;

    while (n--)
        s += *p++;
    return s;
}

int main(void)
{
    static unsigned char ub[] = { 0, 1, 0x80, 0xff };
    static unsigned short us[] = { 0x8000, 0xffff };
    static signed char sb[] = { -1, -128, 127 };

    if (from_uchar(&ub[2]) != 0x80L || from_uchar(&ub[3]) != 0xffL)
        return 1;
    if (from_ushort(&us[0]) != 0x8000L || from_ushort(&us[1]) != 0xffffL)
        return 2;
    if (from_and(-1) != 0xffL || from_and(0x1234) != 0x34L)
        return 3;
    if (from_schar(&sb[0]) != -1L || from_schar(&sb[1]) != -128L
        || from_schar(&sb[2]) != 127L)
        return 4;
    if (sum(ub, 4) != 0x180L)
        return 5;

    return 42;
}
