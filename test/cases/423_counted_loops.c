/* Loops that count, as the machine IR makes them. A counter from a
 * constant no less than 0, stepped up by one, while it is below a
 * constant, stays within the two -- its bits above known 0, `i < 20`
 * compared as a byte -- and an address `base + i * stride` from what the
 * loop does not change is a pointer of its own, set where the loop is
 * entered and stepped where i is. Each shape is here at its edges: a bound
 * at and past a power of two, <=, !=, a constant on the left, a test made
 * the other way round, a loop that never runs, a counter from below 0 or
 * counting down; addresses of ints, shorts, chars and structs whose step
 * is no power of two, in rows and columns, and one read after the counter
 * is stepped, or kept past the loop, which are left as they are; one
 * member read three times, which is one pointer, and two arrays alike,
 * which are two. Not
 * inlined, so that each is made as a loop of its own function. */

int seed[4] = { 3, 5, 7, 11 };

static int in(int k) { return seed[k]; }

struct odd { char tag; int value; short half; };      /* 6 bytes */

int grid[8][9];
short narrow[9][8];
unsigned char bytes[300];
struct odd odds[12];
int a[10];
int b[10];
int wide[4][50];                        /* rows of 150 bytes: past lea's reach */

__attribute__((noinline)) static int count_to(int n)
{
    int sum = 0, i;

    for (i = 0; i < 256; i++)
        sum += i & n;
    return sum + i + (i > 100);         /* i is 256 here */
}

__attribute__((noinline)) static int bounds(void)
{
    int sum = 0, i;

    for (i = 0; i <= 255; i++)
        sum += i;
    sum += i * 1000 + (i > 100);        /* 256001: i is 256, not a byte */
    for (i = 0; i != 300; i++)
        sum += (i < 150) - (i > 260);
    for (i = 3; 20 > i; i++)
        sum += i * in(0);
    for (i = 300; i < 20; i++)
        sum += 1000000;
    sum += i + (i > 100);               /* 301: it never ran */
    for (i = 5; i < 10; i--)
        if (i < -3)
            break;
    sum += (i < 0) * 7;                 /* counted down past 0 */
    for (i = -5; i < 5; i++)
        sum += i * i * i;
    for (i = 19; i >= 0; i--)
        sum += i - 30;
    i = 0;
    while (!(i >= 40))
        i++;
    return sum + i;
}

__attribute__((noinline)) static long matrix(void)
{
    int i, j, k;
    long sum = 0;

    for (i = 0; i < 8; i++)
        for (j = 0; j < 9; j++)
            grid[i][j] = (i + 1) * (j + in(0)) - 20;
    for (i = 0; i < 8; i++)
        for (j = 0; j < 9; j++)
            narrow[j][i] = (short) (grid[i][j] * 100);
    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++) {
            int dot = 0;

            for (k = 0; k < 9; k++)
                dot += grid[i][k] * narrow[k][j];
            sum += dot;
        }
    for (k = 0; k < 300; k++)
        bytes[k] = (unsigned char) (k * in(1));
    for (k = 0; k < 12; k++) {
        odds[k].tag = (char) k;
        odds[k].value = k * in(2);
        odds[k].half = (short) (k - 6);
    }
    for (k = 0; k < 12; k++)
        sum += odds[k].value * odds[k].half + odds[k].tag;
    for (k = 299; k >= 0; k--)
        sum += bytes[k];
    for (k = 7; k >= 0; k--)            /* a column, down: IY stepped by -27 */
        sum += grid[k][in(0)] * (k + 1);
    for (k = 0; k < 4; k++)
        wide[k][k + 10] = k * in(2) + 1;
    for (k = 0; k < 4; k++)
        sum += wide[k][k + 10] * 100;
    return sum;
}

/* What the loops wrote, read where it is, not through their pointers. */
__attribute__((noinline)) static long fixed(void)
{
    return grid[7][8] * 1000000L + narrow[3][5] * 1000L + odds[7].value * 10
           + odds[11].half + bytes[299] + a[9] + wide[3][13];
}

__attribute__((noinline)) static int stepped_first(void)
{
    int k, sum = 0;
    int *last = 0;

    for (k = 0; k < 10; k++)
        a[k] = k * in(3);
    for (k = 0; k < 9; ) {
        int *p = &a[k];

        k++;
        sum += *p + a[k];               /* p made before the step */
    }
    for (k = 0; k < 10; k++)
        b[k] = 100 - k;
    for (k = 0; k < 10; k++)            /* two arrays alike: two pointers */
        sum += a[k] * b[k] - a[k];
    for (k = 0; k < 10; k++)
        last = &a[k];
    sum += *last;                       /* kept past the loop */
    k = 0;
    while (k < 10)
        sum += a[k++] * 2;              /* a[k] made after k is stepped */
    return sum;
}

#define CHECK(expr, want) { check++; if ((long) (expr) != (long) (want)) return check; }

int main(void)
{
    int check = 0;

    CHECK(count_to(in(1)), 640 + 256 + 1)
    CHECK(bounds(), 289126)
    CHECK(matrix(), 15436910)
    CHECK(stepped_first(), 47850)
    CHECK(fixed(), 69600831)

    return 42;
}
