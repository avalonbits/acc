/* Pointers and ints added and taken where one of them lives in BC: the
 * int in BC added as many times as the element's size, or the pointer in
 * BC with the scaled int added to or taken from it -- elements of one,
 * two, three and four bytes. */
static void insertion(int *a, int n)
{
    for (int i = 1; i < n; i++) {
        int v = a[i], j = i - 1;

        while (j >= 0 && a[j] > v) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = v;
    }
}

static void quick(int *a, int n)
{
    while (n > 4) {
        int pivot = a[n / 2], i = 0, j = n - 1;

        while (i <= j) {
            while (a[i] < pivot)
                i++;
            while (a[j] > pivot)
                j--;
            if (i <= j) {
                int t = a[i];

                a[i] = a[j];
                a[j] = t;
                i++;
                j--;
            }
        }
        if (j + 1 < n - i) {
            quick(a, j + 1);
            a += i;
            n -= i;
        } else {
            quick(a + i, n - i);
            n = j + 1;
        }
    }
    insertion(a, n);
}

/* From the end back: the pointer less the index. */
static long sum_back(const long *end, int n)
{
    long sum = 0;

    for (int k = 1; k <= n; k++)
        sum += *(end - k);

    return sum;
}

/* The same with ints: the index in BC taken from the end, and a pointer
 * in BC less a lag. */
static int ints_back(const int *end, int n)
{
    int sum = 0;

    for (int k = 1; k <= n; k++)
        sum += *(end - k) * k;

    return sum;
}

static int lagged(const int *p, const int *end, int lag)
{
    int sum = 0;

    while (p != end) {
        sum += *(p - lag);
        p++;
    }

    return sum;
}

/* An index worked out, so in neither BC nor a slot: int + pointer, the
 * pointer on the right, and a pointer less it. */
static int worked(const int *base, int n)
{
    int sum = 0;

    for (int k = 0; k < n; k++)
        sum += (k ^ 1)[base] * 10 + *(base + n - 1 - (k ^ 1));

    return sum;
}

static int sum_shorts(const short *s, int n)
{
    int sum = 0;

    for (int k = 0; k < n; k++)
        sum += s[k] + *(s + (n - 1 - k));

    return sum;
}

static int sum_bytes(const unsigned char *b, int n)
{
    int sum = 0;

    for (int k = n - 1; k >= 0; k--)
        sum += b[k] - *(b + k);         /* each the same byte */

    return sum + b[n - 1];
}

int main(void)
{
    static int values[40];
    static const long longs[5] = { 1, 20, 300, 4000, 50000 };
    static const short shorts[4] = { -1, 2, -3, 4 };
    static const unsigned char bytes[3] = { 7, 8, 250 };
    unsigned state = 7;
    int right = 0, k, sorted = 1;

    for (k = 0; k < 40; k++) {
        state = state * 75 + 74;
        values[k] = (int) (state % 201) - 100;
    }
    quick(values, 40);
    for (k = 1; k < 40; k++)
        sorted &= values[k - 1] <= values[k];
    right += sorted;
    right += sum_back(longs + 5, 4) == 54320;
    right += sum_shorts(shorts, 4) == 4;
    right += sum_bytes(bytes, 3) == 250;
    {
        static const int ints[5] = { 1, 20, 300, 4000, 50000 };

        /* 50000*1 + 4000*2 + 300*3 + 20*4 */
        right += ints_back(ints + 5, 4) == 58980;
        /* ints[0..2] seen from ints[2..4] */
        right += lagged(ints + 2, ints + 5, 2) == 321;
        /* k ^ 1 over 0..3 is 1, 0, 3, 2: each element ten times, and each
         * from the other end once */
        right += worked(ints, 4) == 43210 + 4321;
    }
    return right == 7 ? 42 : right;
}
