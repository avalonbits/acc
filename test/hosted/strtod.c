/* strtod, strtof and atof against glibc's strtof, bit for bit: decimal and
 * hexadecimal, halfway cases that need every digit, the largest and the
 * smallest there are and just past them, INF and NAN, the endings that are
 * and are not part of a number, and errno. */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *texts[] = {
    "0", "-0", "1", "-1", "0.1", "3.14159265358979", "1e10", "1E-10", "123.456e-7",
    ".5", "5.", "-.25e+2", "  \t12.5xyz", "+7", "1e", "1e+", "1ex", "e5", ".", "-",
    "0x1p0", "-0x1p0", "0x1.8p1", "0X.8P-1", "0x1.fffffep127", "0x1.ffffffp127",
    "0x1p-149", "0x1p-150", "0x1.8p-150", "0x1p-126", "0x0.000002p-126",
    "0x", "0xg", "0x.p1", "0x1p", "0x10", "0xABCDEFp-4",
    "inf", "-INF", "Infinity", "infin", "nan", "NaN(abc_1)", "nan(", "-nan",
    "3.4028235e38", "3.4028236e38", "3.40282357e38", "1e39", "-1e39",
    "1.17549435e-38", "1.4e-45", "7.006492e-46", "7.006493e-46", "1e-46", "1e-50",
    "16777216", "16777217", "16777218", "16777219", "33554435",
    "0.000000000000000000000000000000000000000000001401298464324817",
    "1.00000005960464477539062", "1.000000059604644775390625",
    "1.0000000596046447753906250000000000000000000000000000000001",
    "1.00000017881393432617187499999999999999999999999999999999999",
    "340282356779733661637539395458142568447.9999",
    "340282356779733661637539395458142568448",
    "0.00000000000000000000000000000000000001175494280757364291727671",
    "000000000000000000000000000000000000000000000000000012345",
    "12345678901234567890123456789012345678901234567890e-40",
    "1e-38", "9.9999999e-39", "2e-45", "3e-45",
};

int main(void)
{
    int i;

    for (i = 0; i < (int) (sizeof texts / sizeof *texts); i++) {
        char *end;
        float f;

        errno = 0;
        f = strtof(texts[i], &end);
        if (isnan(f))
            printf("%d: nan", i);
        else
            printf("%d: %a", i, f);
        printf(" end %d errno %d\n", (int) (end - texts[i]), errno == ERANGE);
    }

    /* strtod and atof, where a double rounds to the same float as the
     * float itself does, so that the host's wider double agrees. */
    printf("%a %a %a\n", (float) strtod("2.5e3x", NULL), (float) atof(" -0.375"),
           (float) atof("1e1000"));
    {
        char *end;
        double d = strtod("  0x1.8p-2 rest", &end);

        printf("%a [%s]\n", (float) d, end);
    }

    /* Random bit patterns, written out with too few digits, just enough,
     * and many more, and read back. The same generator on both machines,
     * so the same numbers. */
    {
        unsigned long seed = 12345;
        char buf[80];
        int bad = 0, n;

        for (n = 0; n < 150; n++) {
            union { float f; unsigned long u; } v;
            static const char *const forms[] = { "%.6g", "%.9g", "%.17e", "%.3e" };
            int k;

            seed = (seed * 1103515245UL + 12345UL) & 0xffffffffUL;
            v.u = (seed ^ (seed >> 11) << 5) & 0xffffffffUL;
            if (((v.u >> 23) & 0xff) == 0xff)
                continue;
            for (k = 0; k < 4; k++) {
                sprintf(buf, forms[k], (double) v.f);
                printf("%s %a\n", buf, strtof(buf, NULL));
            }
        }
    }

    return 0;
}
