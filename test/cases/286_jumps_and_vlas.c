/* Jumps that stay out of a VLA's scope, or leave it, which C99 allows
 * (6.8.6.1p1 forbids only jumping in): to a label before the array's
 * declaration, out of its block, around a block that has one, and a switch
 * whose case follows a block with one inside it. */
static int count(int n)
{
    int total = 0, i = 0;

again:
    {
        int a[n];

        a[0] = i;
        total += a[0];
        if (++i < 3)
            goto again;                 /* back, out of a's scope */
        goto done;                      /* forward, out of it */
    }
done:
    return total;
}

static int pick(int k, int n)
{
    switch (k) {
    case 1:
        {
            int b[n];

            b[0] = 30;
            return b[0];
        }
    default:
        ;
        int c[n];

        c[0] = 9;
        return c[0];
    }
}

int main(void)
{
    goto skip;                          /* around a block with one */
    {
        int d[2];

        d[0] = 1;
        return d[0];
    }
skip:
    return count(2) + pick(1, 2) + pick(0, 2);
}
