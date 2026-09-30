/* A local read, and the value kept, before a call that changes the local
 * through its address: `int z = y; b(&y); c(z, y);` wants y as it was in
 * z. opt-acc's SSA form leaves a value read once on the classic stack for
 * its reader, and the classic backend reads a local lazily -- where the
 * value is used, after the call. gcc's 20010403-1. */
static int seen;

static void bump(int *y) { (*y)++; }

static void check(int x, int y)
{
    seen = x * 10 + y;
}

static void f(int x, int y)
{
    int z = y;

    (void) x;
    bump(&y);
    check(z, y);
}

static int g(int *p)
{
    int before = *p;

    bump(p);
    return before * 10 + *p;
}

int main(void)
{
    int v = 4;

    f(0, 3);
    if (seen != 34)
        return 1;
    if (g(&v) != 45)
        return 2;

    return 42;
}
