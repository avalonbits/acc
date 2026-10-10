/* A comparison as the condition of ?:, with values live under it in
 * registers -- the left of an add, an index, a call's earlier argument:
 * the jump made on the comparison's flags, the values spilled between,
 * as the 1 or 0 is no longer made and tested again. Signed and unsigned,
 * each order, at the edges, nested, and with the values read back after. */

typedef unsigned char u8;

const u8 table[8] = { 10, 11, 12, 13, 14, 15, 16, 17 };

__attribute__((noinline)) int two(int a, int b)
{
    return a * 1000 + b;
}

__attribute__((noinline)) int add_u8(int x, u8 c)
{
    return x * 3 + (c < 26 ? 1 : 2);
}

__attribute__((noinline)) int add_signed(int x, int y, int n)
{
    return (x - y) + (n >= -5 ? x : y);
}

__attribute__((noinline)) int add_unsigned(int x, unsigned u)
{
    return (x << 1) + (u > 0x800000u ? 7 : 9);
}

__attribute__((noinline)) int index_of(int x, int k)
{
    return table[(x & 3) + (k <= 2 ? 1 : 4)];
}

__attribute__((noinline)) int arg(int x, int k)
{
    return two(x + 1, k != 3 ? x : -x);
}

__attribute__((noinline)) int nested(int x, int a, int b)
{
    return x * 5 + (a < b ? (a == 0 ? 100 : 200) : (b > 7 ? 300 : 400));
}

/* A call in one arm, which would move a value left in a register there
 * and not on the other path. */
__attribute__((noinline)) int call_arm(int x, int k)
{
    return x * 3 + (k < 4 ? two(k, x) : x + 5) + x * 7;
}

__attribute__((noinline)) long wide(long w, int n)
{
    return w + (n < 0 ? 1L : 2L);
}

int main(void)
{
    if (add_u8(5, 25) != 16 || add_u8(5, 26) != 17 || add_u8(5, 255) != 17)
        return 1;
    if (add_signed(10, 3, -5) != 17 || add_signed(10, 3, -6) != 10)
        return 2;
    if (add_signed(-100, 50, 8388607) != -250 || add_signed(-100, 50, -8388608) != -100)
        return 3;
    if (add_unsigned(4, 0x800001u) != 15 || add_unsigned(4, 0x800000u) != 17)
        return 4;
    if (index_of(6, 2) != 13 || index_of(6, 3) != 16)
        return 5;
    if (arg(9, 1) != 10009 || arg(9, 3) != 9991)
        return 6;
    if (nested(1, 0, 1) != 105 || nested(1, 2, 3) != 205
        || nested(1, 9, 8) != 305 || nested(1, 9, 7) != 405)
        return 7;
    /* The path without the call first, so that its slot holds nothing
     * an earlier call left there. */
    if (call_arm(9, 4) != 27 + 14 + 63 || call_arm(2, 3) != 6 + 3002 + 14)
        return 9;
    if (wide(100000L, -1) != 100001L || wide(100000L, 0) != 100002L)
        return 8;

    return 42;
}
