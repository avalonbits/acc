/* A local declared register lives in IY, and a value stored into it is
 * converted to its type first, as for any other local: a float truncated
 * toward zero, a long or long long taken to its low bits. The store to IY
 * once came before the conversion, and a float on the right was refused as
 * "not implemented". Each is checked against the same arithmetic on a local
 * in the frame.
 */
static int from_float(register int x, float y)
{
    x += y;

    return x;
}

static int from_float_plain(int x, float y)
{
    x += y;

    return x;
}

static int from_double(register int x, double y)
{
    x = y;

    return x - 1;
}

static int from_long(register int x, long y)
{
    x += y;

    return x;
}

static int from_llong(register int x, long long y)
{
    x *= y;

    return x;
}

int main(void)
{
    int bad = 0;

    bad += from_float(10, 2.75f) != 12;
    bad += from_float(10, -12.5f) != -2;
    bad += from_float(3, 0.5f) != from_float_plain(3, 0.5f);
    bad += from_double(0, -7.9) != -8;
    bad += from_long(1, 0x123456L) != 0x123457;
    bad += from_long(1, 0x7f000001L) != 2;          /* the low 24 bits */
    bad += from_llong(3, 1000000LL) != 3000000;

    return bad ? 100 + bad : 42;
}
