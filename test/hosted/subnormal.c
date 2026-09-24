/* Float arithmetic with subnormal operands and results: the runtime's
 * multiply and divide took a denormal's significand as though its leading
 * bit were at the top, and 1e-40f * 8 came back as 1e-40f. Every pair of
 * a spread of values, through all four operations, bit for bit. */
#include <stdio.h>

static const float vals[] = {
    1.4e-45f, 2.8e-45f, 1e-44f, 3.3e-42f, 1e-40f, 5.87747e-39f,
    1.1754942e-38f, 1.17549435e-38f, 2.5e-38f, 1e-30f, 0.5f, 1.0f,
    3.0f, 0.1f, 7.5e5f, 1e30f, 3e38f, -1e-40f, -1.4e-45f, -3.0f,
};
#define N ((int) (sizeof vals / sizeof *vals))

int main(void)
{
    volatile float a, b;
    int i, j;

    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++) {
            a = vals[i];
            b = vals[j];
            printf("%a %a: %a %a %a %a\n", a, b, a * b, a / b, a + b, a - b);
        }

    return 0;
}
