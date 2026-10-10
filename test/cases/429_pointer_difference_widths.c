/* The difference of two pointers, divided by the element's width exactly:
 * a shift for the twos in it and a multiply by the inverse of the odd rest,
 * modulo 2^24. Widths odd, even and both, small and large, each difference
 * forwards and backwards and across most of an array; and a VLA's row,
 * whose width is known only when it runs, which still divides. */

struct w3 { char c[3]; };
struct w5 { char c[5]; };
struct w6 { char c[6]; };
struct w7 { char c[7]; };
struct w12 { char c[12]; };
struct w15 { char c[15]; };
struct w40 { char c[40]; };
struct w255 { char c[255]; };
struct w1000 { char c[1000]; };

struct w3 a3[100];
struct w5 a5[100];
struct w6 a6[100];
struct w7 a7[100];
struct w12 a12[100];
struct w15 a15[100];
struct w40 a40[100];
struct w255 a255[30];
struct w1000 a1000[20];
struct w3 big[40001];
int ints[100];
char *ptrs[100];
long longs[100];

int at[4] = { 0, 1, 17, 99 };

__attribute__((noinline)) int pick(int k)
{
    return at[k];
}

/* Every pair of the indexes, each way round: the difference back, and its
 * sum over all of them as the check. */
#define PAIRS(arr, limit)                                                \
    for (i = 0; i != 4; i++)                                             \
        for (j = 0; j != 4; j++) {                                       \
            int x = pick(i) % (limit), y = pick(j) % (limit);            \
            if (&arr[x] - &arr[y] != x - y)                              \
                return 1;                                                \
            sum += &arr[x] - &arr[y];                                    \
        }

__attribute__((noinline)) int differences(void)
{
    int i, j, sum = 0;

    PAIRS(a3, 100)
    PAIRS(a5, 100)
    PAIRS(a6, 100)
    PAIRS(a7, 100)
    PAIRS(a12, 100)
    PAIRS(a15, 100)
    PAIRS(a40, 100)
    PAIRS(a255, 30)
    PAIRS(a1000, 20)
    PAIRS(ints, 100)
    PAIRS(ptrs, 100)
    PAIRS(longs, 100)

    return sum == 0 ? 0 : 2;
}

__attribute__((noinline)) int rows(int n, int x, int y)
{
    int m[n][n];

    return &m[x] - &m[y];
}

/* A difference past 2^15 elements, where a wrong inverse shows in the top
 * byte: three-byte elements 40000 apart are 120000 bytes. */
__attribute__((noinline)) int far(struct w3 *p, int n)
{
    return (p + n) - p;
}

int main(void)
{
    if (differences())
        return 1;
    if (rows(7, 5, 2) != 3 || rows(7, 2, 5) != -3 || rows(4, 3, 0) != 3)
        return 2;
    if (far(big, 40000) != 40000 || far(big + 40000, -40000) != -40000)
        return 3;
    if (&ints[3] - &ints[90] != -87 || &a7[99] - a7 != 99)
        return 4;

    return 42;
}
