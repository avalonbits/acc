/* Two longs a loop carries in registers, E:UHL and A:UBC, swapped where
 * the loop goes round: the copies made at once, a long's pair through the
 * stack and its top byte as a byte, E and A swapped through a byte free
 * for it -- each of all four bytes, which the values here differ in. */
static long swap_loop(long a, long b, int n)
{
    while (n--) {
        long t = a;

        a = b;
        b = t;
    }
    return a;
}

static long rotate(long a, long b, int n)
{
    while (n--) {
        long t = a + b;

        a = b;
        b = t;
    }
    return a - b;
}

int main(void)
{
    int r = 0;

    r += swap_loop(0x11223344L, 0x55667788L, 3) == 0x55667788L;
    r += swap_loop(0x11223344L, 0x55667788L, 4) == 0x11223344L;
    r += rotate(0x01000000L, 0x02000000L, 5) == (long) (0x0d000000L - 0x15000000L);
    return r * 14;
}
