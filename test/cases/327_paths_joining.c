/* Locals whose values meet where paths join: a loop's counter, one set on
 * only some trips, a condition that jumps straight into the loop's step,
 * and one that jumps out to the end of the function past everything
 * else. opt-acc with OPTACC_SSA keeps these as values, and copies each
 * onto the path it joins by; nothing but the right path may reach them.
 */
static unsigned long h = 1;

static void mix(unsigned long v)
{
    h = ((h ^ (h >> 13)) * 31 + v) & 0xffffffffUL;
}

static int bad;

static void fail(void)
{
    bad++;
}

/* gcc's loop-7: the jump when `j < 0` is false goes to the function's end. */
static void find_bit(unsigned n)
{
    int i, j = -1;

    for (i = 0; i < 10 && j < 0; i++) {
        if ((1UL << i) == n)
            j = i;
    }
    if (j < 0)
        fail();
}

static int first_over(const int *v, int n, int limit)
{
    int i, at = -1;

    for (i = 0; i < n; i++) {
        if (v[i] <= limit)
            continue;
        at = i;
        break;
    }
    if (at < 0)
        return -1;

    return at * 10 + v[at];
}

static int total;

/* The loop's jump back is the last thing: its copies are made after it,
 * and the end of the function has to jump over them. */
static void count_down(int n)
{
    do {
        total += n;
        n--;
    } while (n > 0);
}

static void baz(void)
{
}

/* An address kept in a local narrower than an address. */
static int narrow_address(int n)
{
    unsigned short c;
    int trips = 0;

    for (c = 0; c; c = (unsigned) baz)
        trips++;

    return trips + n;
}

int main(void)
{
    static const int v[] = { 3, 9, 1, 12, 4 };
    int i;

    find_bit(64);
    find_bit(512);
    find_bit(3);
    mix((unsigned long) bad);
    for (i = 0; i < 14; i += 3)
        mix((unsigned long) first_over(v, 5, i));
    mix((unsigned long) narrow_address(5));
    count_down(6);
    count_down(-2);
    mix((unsigned long) total);

    return h % 200 == 68 ? 42 : 1;
}
