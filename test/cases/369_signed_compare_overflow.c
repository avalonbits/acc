/* Signed comparisons of two variables where the difference overflows --
 * the ends of the range against each other and against small values --
 * and where it does not. The sign of a - b is the answer only when the
 * subtract did not overflow; opt-acc's machine-level backend puts the sign
 * in the carry and turns the carry over when it did. With a loop that
 * keeps its counter and its total in registers across the comparison. */
#include <limits.h>

int lt(int a, int b) { return a < b; }
int ge(int a, int b) { return a >= b; }
int gt(int a, int b) { return a > b; }
int le(int a, int b) { return a <= b; }

int count_up(int from, int to)
{
    int i, n = 0;

    for (i = from; i < to; i++)
        n += i & 1;

    return n;
}

int main(void)
{
    int r = 0;

    r += lt(INT_MIN, INT_MAX) && !lt(INT_MAX, INT_MIN);
    r += lt(INT_MIN, 1) && !lt(1, INT_MIN) && lt(-1, INT_MAX);
    r += !lt(INT_MAX, -1) && lt(-1, 0) && !lt(0, -1) && !lt(5, 5);
    r += ge(INT_MAX, INT_MIN) && !ge(INT_MIN, INT_MAX) && ge(5, 5);
    r += gt(1, INT_MIN) && !gt(INT_MIN, 1) && !gt(-7, -7);
    r += le(INT_MIN, -1) && !le(INT_MAX, INT_MIN) && le(-7, -7);
    r += count_up(-5, 6) == 6 && count_up(3, 3) == 0 && count_up(INT_MAX - 3, INT_MAX) == 1;

    return r == 7 ? 42 : r;
}
