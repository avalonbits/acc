/* `[*]` in a prototype's parameter: a length that varies and is not said
 * (C99 6.7.5.2p4), with qualifiers in front of it or not. A parameter's
 * first dimension is a pointer whatever it says, so these agree with
 * definitions that take pointers. acc read the `*` as the start of a size
 * and refused the `]`; c-testsuite's 00162 has it. */
int sum(int n, int a[*]);
int first(int a[const *]);
int twice(int n, int (*get)(int a[*]));

int sum(int n, int a[])
{
    int s = 0;

    while (n--)
        s += a[n];

    return s;
}

int first(int *const a)
{
    return *a;
}

int main(void)
{
    int v[3] = { 10, 12, 20 };

    return sum(3, v) - first(v) + 10;   /* 42 */
}
