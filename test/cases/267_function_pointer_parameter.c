/* A parameter that is a pointer to a function has a parameter list of its
 * own, and acc added that list's types to its table in the middle of the
 * outer function's: `take(double (*f)(double), long k)` read f's double as
 * take's first parameter and f as its second. A call then converted the
 * function it was given to a float, which was refused (921208-1,
 * 20021118-2), and k -- converted to a pointer -- lost its top byte. */
double sq(double x) { return x * x; }
int twice(int x) { return 2 * x; }

static long take(double (*f)(double), long k)
{
    return (*f)(3.0) == 9.0 ? k : -1;
}

static int apply(int (*g)(int), long k, int (*h)(int))
{
    return k == 0x1000000L ? g(h(5)) : -1;
}

int main(void)
{
    int r = 0;

    if (take(sq, 0x1000002L) == 0x1000002L) r++;
    if (take(&sq, -5L) == -5L) r++;
    if (apply(twice, 0x1000000L, twice) == 20) r++;

    return r + 39;              /* 3 checks */
}
