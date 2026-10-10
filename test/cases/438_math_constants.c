/* INFINITY, HUGE_VAL, HUGE_VALF and NAN are constants, as C99 7.12 has
 * them: statics initialised to them, as Berry's math module's table is --
 * the infinities past every finite float, of the types they are said to
 * be, and the same whichever is asked; the NaN unequal to itself. */
#include <math.h>

static const float inf = INFINITY;
static const double huge = HUGE_VAL;
static const float hugef = HUGE_VALF;
static const float table[2] = { -INFINITY, 1.5f };
static const float nan_value = NAN;

int main(void)
{
    volatile float largest = 3.4028234e38f;

    if (!(inf > largest) || !(huge > largest) || !(hugef > largest))
        return 1;
    if (!(table[0] < -largest) || table[1] != 1.5f)
        return 2;
    if (inf != hugef || inf != huge || !isinf(inf) || inf - inf == inf - inf)
        return 3;
    if (sizeof INFINITY != sizeof(float) || sizeof HUGE_VAL != sizeof(double))
        return 4;
    if (nan_value == nan_value || !isnan(nan_value) || isnan(inf))
        return 5;

    return 42;
}
