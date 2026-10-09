/* Ints shifted right by every constant from 1 to 23, signed and unsigned:
 * the SSA form's backends make these as the first pass does, the runtime's
 * shr entered for the count or HL's top byte through the stack, where they
 * called the routine that loops over the count. */

static const int vals[] = {
    0, 1, -1, 2, 8388607, -8388608, 1193046, -1193046, 65280, -65536, -8388607, 255, 256, -256, 5921370, 3031952, 3550194, 4873875, -8270740, -6099583, -6397452, 3696082, 5800100, -2808386, -2140590, -6847939, -3104617, -162553, 1872588, 121492, -2413561, 2453531, -7832287, 7160214, -2531661
};

#define ONE(k) sum = sum * 31 + (unsigned long) ((unsigned) (v >> k) & 0xffffffu); \
    sum = sum * 31 + (unsigned long) ((unsigned) v >> k);

int main(void)
{
    unsigned long sum = 0;
    int i;

    for (i = 0; i < (int) (sizeof vals / sizeof vals[0]); i++) {
        int v = vals[i];

        ONE(1) ONE(2) ONE(3) ONE(4) ONE(5) ONE(6) ONE(7) ONE(8) ONE(9) ONE(10) ONE(11) ONE(12)
        ONE(13) ONE(14) ONE(15) ONE(16) ONE(17) ONE(18) ONE(19) ONE(20) ONE(21) ONE(22) ONE(23)
    }

    return sum == 4108577662UL ? 42 : 1;
}
