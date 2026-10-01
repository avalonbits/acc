/* Conditions that are constants: the branch is taken always or never, with
 * nothing tested, and the loops still leave by break and continue. */
#define BUMP(v) do { (v)++; } while (0)

static int loops(int n)
{
    int count = 0, i = 0;

    while (1) {
        if (i == n)
            break;
        i++;
        BUMP(count);
    }
    do {
        count += 100;
        if (n)
            continue;               /* to the test, which is false */
        count += 1000;
    } while (0);
    for (;;) {
        if (--i < 0)
            break;
        count += 10000;
    }

    return count;
}

static int choices(int x)
{
    if (0)
        x += 1;
    else
        x += 2;
    if (1)
        x += 4;
    else
        x += 8;
    if (0L)
        x += 16;
    if (!0)
        x += 32;

    return x;
}

int main(void)
{
    int right = 0;

    right += loops(3) == 30103;
    right += loops(0) == 1100;
    right += choices(0) == 38;
    return right == 3 ? 42 : right;
}
