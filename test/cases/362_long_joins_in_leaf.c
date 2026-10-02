/* Longs in opt-acc's leaf backend where the paths join and where values
 * wait: a branch on a long whose only bit set is in its top byte; two
 * longs a loop carries into its next turn together, copied at once into
 * their phis' slots, each all four bytes; a division kept a long, whose
 * right operand goes through the stack, with an int waiting on the stack
 * around it; and a shift right narrowed to an int, which keeps bits a
 * long's top byte brings down and so is not worked out on the low three
 * bytes. */
int top_only(long x)
{
    if (x)
        return 1;
    return 0;
}

long fib(int n)
{
    long a = 0, b = 1;

    while (n--) {
        long next = a + b;

        a = b;
        b = next;
    }

    return a;
}

int waiting(int i, long a, long b)
{
    return i * 3 + (int) ((a / b) >> 1);
}

int shifted(long x)
{
    return (int) (x >> 8);
}

int main(void)
{
    int r = 0;

    r += top_only(0x1000000L) == 1 && top_only(0L) == 0;
    r += fib(40) == 102334155L && fib(1) == 1L;
    r += waiting(5, 1000000000L, 1000L) == 15 + 500000;
    r += shifted(0x12345678L) == 0x123456;

    return r == 4 ? 42 : r;
}
