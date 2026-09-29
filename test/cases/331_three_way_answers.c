/* The answer of ?: -- here a comparison's -1, 0 or 1 -- is made in more
 * than one place, and each makes it differently: a 1 is a byte, a -1 is
 * not. opt-acc's own backend keeps what each value is held as, to leave
 * out a narrowing it has made already, and read the 1's for all of them:
 * `c == -1` was compared as an unsigned byte. gcc's pr94589-3. */
static int minus_one(int i, int j)
{
    int c = i == j ? 0 : i < j ? -1 : 1;

    return c == -1;
}

static int above_minus_one(int i, int j)
{
    int c = i == j ? 0 : i < j ? -1 : 1;

    return c > -1;
}

static int below_one(int i, int j)
{
    int c = i == j ? 0 : i < j ? -1 : 1;

    return c < 1;
}

static int order(int i, int j)
{
    return minus_one(i, j) + 2 * above_minus_one(i, j) + 4 * below_one(i, j);
}

int main(void)
{
    int r = order(7, 8) * 100 + order(8, 8) * 10 + order(9, 8);

    /* 7 < 8: -1, so 1 + 4 = 5; equal: 0, so 2 + 4 = 6; 9 > 8: 2. */
    return r == 5 * 100 + 6 * 10 + 2 ? 42 : 1;
}
