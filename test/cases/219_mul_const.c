/* Multiplication by a constant, as doublings and additions.
 *
 * Every multiply zap executes has a constant on the right, and the commonest
 * is by 13 -- the width of a struct it keeps an array of. So acc writes the
 * constant ones out: start with x, and for each bit below the top one,
 * double what is in hand and add x back where that bit is set.
 *
 * The constants here cover each shape that takes: a power of two (doublings
 * alone, no copy of x needed), a run of set bits, a sparse one, the top and
 * bottom of the range, and constants big enough that acc gives up and calls
 * the helper instead. An int here is twenty-four bits, so the products are
 * checked for wrapping as well.
 *
 * The part no simple case reaches is that DE is borrowed to hold x and
 * handed back: the multiply is put in the middle of expressions that have
 * other values in flight, so a multiply that ate DE would show up as a wrong
 * answer rather than not at all.
 */
static int id(int x) { return x; }

static int m0(int x) { return x * 0; }
static int m1(int x) { return x * 1; }
static int m2(int x) { return x * 2; }
static int m3(int x) { return x * 3; }
static int m4(int x) { return x * 4; }
static int m5(int x) { return x * 5; }
static int m7(int x) { return x * 7; }
static int m8(int x) { return x * 8; }
static int m10(int x) { return x * 10; }
static int m13(int x) { return x * 13; }
static int m15(int x) { return x * 15; }
static int m16(int x) { return x * 16; }
static int m255(int x) { return x * 255; }
static int m256(int x) { return x * 256; }
static int m4096(int x) { return x * 4096; }
static int mneg(int x) { return x * -3; }

static unsigned um13(unsigned x) { return x * 13u; }

/* DE is live across the multiply here: both sides are computed and combined,
 * so a multiply that clobbered it would lose one of them. */
static int mixed(int a, int b) { return a * 13 + b * 5; }
static int nested(int a, int b) { return (a * 3) * (b * 7); }
static int with_index(const unsigned char *p, int i) { return p[i * 13]; }

int main(void)
{
    int x = id(7);
    int big = id(0x4321);

    if (m0(x) != 0) return 1;
    if (m1(x) != 7) return 2;
    if (m2(x) != 14) return 3;
    if (m3(x) != 21) return 4;
    if (m4(x) != 28) return 5;
    if (m5(x) != 35) return 6;
    if (m7(x) != 49) return 7;
    if (m8(x) != 56) return 8;
    if (m10(x) != 70) return 9;
    if (m13(x) != 91) return 10;
    if (m15(x) != 105) return 11;
    if (m16(x) != 112) return 12;
    if (m255(x) != 1785) return 13;
    if (m256(x) != 1792) return 14;
    if (m4096(x) != 28672) return 15;
    if (mneg(x) != -21) return 16;

    /* Bigger operands, where the product uses the whole width. */
    if (m13(big) != 0x368ad) return 17;
    if (m3(big) != 0xC963) return 18;
    if (um13(0x100000u) != 0xd00000u) return 19;

    /* Negative left operands: the doublings are the same bits either way. */
    if (m13(-7) != -91) return 20;
    if (m5(-1) != -5) return 21;

    if (mixed(3, 4) != 3 * 13 + 4 * 5) return 22;
    if (nested(3, 4) != (3 * 3) * (4 * 7)) return 23;

    {
        static const unsigned char row[64] = { 0 };
        if (with_index(row, 2) != 0) return 24;
    }

    return 42;
}
