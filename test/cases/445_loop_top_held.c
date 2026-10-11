/* A loop whose counter was stored just before its top: the condition's
 * load of it is left out, on the promise that every jump back to the top
 * brings the counter in the register too. Each loop here comes back some
 * way that does not -- the body ends storing something else, a continue
 * leaves from the middle -- so the jump back has to load it first, or the
 * condition tests whatever the register was left holding. */
static int calls;

static int seven(int x)
{
    calls++;

    return 7 + x * 0;
}

int main(void)
{
    int i = 0, n = 5, sum = 0, j, count = 0;

    /* The body ends storing sum: the register holds 7, 14, ... */
    while (i < n) {
        i++;
        sum += seven(i);
    }
    if (i != 5 || sum != 35 || calls != 5)
        return 1;

    /* A continue that stores something else last, in a for with no step:
     * it jumps straight to the top. */
    sum = 0;
    for (i = 0; i < 10;) {
        if (i & 1) {
            i += 3;
            sum += 100;
            continue;
        }
        i++;
        sum++;
    }
    if (i != 12 || sum != 303)
        return 2;

    /* A while's continue, the same. */
    i = 0;
    sum = 0;
    while (i < 8) {
        i++;
        if (i < 4) {
            sum += seven(i);
            continue;
        }
        sum += 2;
    }
    if (i != 8 || sum != 31)
        return 3;

    /* Nested: each top held, and the outer one's way back is its step. */
    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            count += seven(j);
    if (count != 84)
        return 4;

    /* A step that stores the counter through a call, and a body that
     * leaves another local in the register. */
    sum = 0;
    for (i = 0; i < 20; i += seven(i))
        sum = sum * 2 + 1;
    if (i != 21 || sum != 7)
        return 5;

    return 42;
}
