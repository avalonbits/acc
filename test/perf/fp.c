/* Floating point: a Mandelbrot set of 40x24 points, iterated to 40, and a
 * running sum of squares and quotients -- float additions, multiplications,
 * comparisons and conversions, each a helper call on this machine. */
#include "perf.h"

int main(void)
{
    float scale = 3.0f / (40.0f + (float) (perf_seed & 1));
    unsigned long check = 0;
    float sum = 0.0f;

    perf_start();
    for (int row = 0; row < 24; row++) {
        for (int column = 0; column < 40; column++) {
            float cr = column * scale - 2.0f, ci = row * 0.1f - 1.2f;
            float zr = 0.0f, zi = 0.0f;
            int n = 0;

            while (n < 40 && zr * zr + zi * zi < 4.0f) {
                float t = zr * zr - zi * zi + cr;

                zi = 2.0f * zr * zi + ci;
                zr = t;
                n++;
            }
            check = check * 3 + (unsigned long) n;
        }
    }
    for (int i = 1; i <= 200; i++)
        sum += (float) i * 0.5f / (float) (i + 1);
    check += (unsigned long) (sum * 16.0f);
    perf_stop();

    perf_check(check);

    return 0;
}
