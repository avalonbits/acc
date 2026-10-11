/* A char stored from A and read back at once: the load left out, A still
 * holding it -- unless something jumps in between, or the store's value
 * was the assignment's and was widened, or the slot is another one. And
 * a char stored to a global from a byte just read, not widened first; and
 * a char returned without loading A from L again. */
static int calls;
static signed char gc;
static unsigned char gu;

static int take(int x)
{
    calls++;

    return x;
}

static signed char back(const signed char *p)
{
    return *p;
}

int main(void)
{
    static signed char bytes[] = { -7, 100, -128, 5 };
    static unsigned char ubytes[] = { 250, 3 };
    signed char *p = bytes, d, e;
    unsigned char *q = ubytes, u;
    int n = take(1), r;

    d = *p;
    if (take(d) != -7)
        return 1;
    d = p[1];
    e = p[3];           /* A holds 5 */
    if (n == 0)
        d = p[2];       /* not taken: the join before the read is reached */
    if (take(d) != 100) /* with A still 5 */
        return 2;
    r = take(e = p[3]) + e;
    if (r != 10)
        return 3;
    d = p[0];
    e = p[1];           /* another slot stored last */
    if (take(d) + take(e) != 93)
        return 4;
    u = *q;
    if (take(u) != 250)
        return 5;
    gc = p[2];
    gu = q[0];
    if (gc != -128 || gu != 250)
        return 6;
    if ((gc = p[0]) != -7 || take(gc = p[1]) != 100)
        return 7;
    if (back(p) != -7 || back(p + 2) != -128 || back(p + 1) + 1 != 101)
        return 8;

    return 42;
}
