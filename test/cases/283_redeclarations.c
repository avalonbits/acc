/* What may be declared twice, and where a name may be declared again:
 * - an extern in a block, again, since it has linkage (C99 6.7p3);
 * - a parameter's name in a block inside the body, which is a scope of
 *   its own, as is a for loop's body inside its first clause's. */
int e = 40;

static int shadow(int x)
{
    {
        int x = 1;

        e += x;
    }

    return x;
}

int main(void)
{
    extern int e;
    extern int e;
    int sum = 0;

    for (int i = 0; i < 1; i++) {
        int i = 5;

        sum += i;
    }

    return shadow(sum) - 4 + e;
}
