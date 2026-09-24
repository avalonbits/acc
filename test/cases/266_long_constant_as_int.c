/* An integer constant expression of type long where an int is wanted -- an
 * enum's value, an array's size -- is still one (C99 6.6p6), and fine when
 * its value fits. <limits.h>'s INT_MIN is `(-8388608)`: 8388608 does not
 * fit an int, so it is a long until the minus makes it fit. acc refused
 * any constant held wider than an int. enum-3 is the torture test. */
#define INT_MIN (-8388608)     /* as <limits.h> has them */
#define INT_MAX 8388607

enum e { lowest = INT_MIN, five = 5L, top = (long) INT_MAX };

int x[2L];
int y[(long) 3 + 0ULL];

int main(void)
{
    int r = 0;
    int z[4L];

    if (lowest < 0 && lowest == -8388607 - 1) r++;
    if (five == 5 && top == 8388607) r++;
    if (sizeof x / sizeof x[0] == 2 && sizeof y / sizeof y[0] == 3) r++;
    if (sizeof z / sizeof z[0] == 4) r++;

    return r + 38;              /* 4 checks */
}
