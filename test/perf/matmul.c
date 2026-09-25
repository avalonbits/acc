/* Integer matrices: 20x20 ints multiplied, and a matrix of shorts
 * transposed and summed -- two-dimensional arrays, nested loops and
 * multiplication, which this machine has no instruction for past a byte. */
#include "perf.h"

#define N 20

static int left[N][N], right[N][N], product[N][N];
static short narrow[N][N];

int main(void)
{
    unsigned long state = perf_seed, check = 0;

    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            state = state * 69069UL + 1;
            left[i][j] = (int) (state >> 20 & 63) - 32;
            state = state * 69069UL + 1;
            right[i][j] = (int) (state >> 20 & 63) - 32;
        }

    perf_start();
    for (int round = 0; round < 2; round++) {
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++) {
                int sum = 0;

                for (int k = 0; k < N; k++)
                    sum += left[i][k] * right[k][j];
                product[i][j] = sum;
            }
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++)
                narrow[j][i] = (short) (product[i][j] >> 2);
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++)
                left[i][j] = narrow[i][j] & 63;
    }
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            check = check * 7 + (unsigned long) (product[i][j] & 0xffff);
    perf_stop();

    perf_check(check);

    return 0;
}
