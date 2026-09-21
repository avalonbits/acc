/* Bitwise operators with a constant on the right.
 *
 * These are written out in place rather than called for, and which code goes
 * down depends on the bytes of the constant: a byte that is the operator's
 * identity is skipped, a mask that keeps nothing above the low byte clears
 * the rest of HL with sbc hl, hl, and a constant that reaches into the third
 * byte still goes to the helper because there is no way to name that byte.
 *
 * So the constants here are chosen to land in each of those, and on both
 * sides of each edge: nothing, the low byte alone, the low two, all three,
 * and the identity. An int here is twenty-four bits, so 0xffffff is -1 and a
 * mask of 0xff0000 keeps only the top byte.
 *
 * The answers are checked against agondev, which is what makes this worth
 * more than my own arithmetic.
 */
static int wide(void) { return 0x123456; }
static int neg(void) { return -1; }

static int and_low(int x) { return x & 0x20; }
static int and_one(int x) { return x & 1; }
static int and_zero(int x) { return x & 0; }
static int and_byte(int x) { return x & 0xff; }
static int and_keep_all(int x) { return x & 0xffffff; }
static int and_top_ff(int x) { return x & 0xff00ff; }
static int and_mid(int x) { return x & 0xffff00; }
static int and_two(int x) { return x & 0xffff; }
static int and_hi(int x) { return x & 0xff0000; }

static int or_zero(int x) { return x | 0; }
static int or_one(int x) { return x | 1; }
static int or_mid(int x) { return x | 0xff00; }
static int or_both(int x) { return x | 0xffff; }
static int or_hi(int x) { return x | 0xff0000; }

static int xor_zero(int x) { return x ^ 0; }
static int xor_low(int x) { return x ^ 0xff; }
static int xor_mid(int x) { return x ^ 0xff00; }
static int xor_hi(int x) { return x ^ 0xff0000; }

static unsigned uand(unsigned x) { return x & 0x0f; }
static unsigned ushr_after(unsigned x) { return (x & 0xff0000) >> 16; }

int main(void)
{
    int w = wide();             /* 0x123456, through a call so it is not
                                 * folded away at compile time */
    int n = neg();              /* -1, every bit set */

    if (and_low(w) != 0) return 1;
    if (and_one(w) != 0) return 2;
    if (and_one(w | 1) != 1) return 3;
    if (and_zero(w) != 0) return 4;
    if (and_byte(w) != 0x56) return 5;
    if (and_keep_all(w) != 0x123456) return 6;
    if (and_top_ff(w) != 0x120056) return 7;
    if (and_mid(w) != 0x123400) return 8;
    if (and_two(w) != 0x3456) return 9;
    if (and_hi(w) != 0x120000) return 10;

    /* Every bit set, so a mask answers with itself. */
    if (and_byte(n) != 0xff) return 11;
    if (and_keep_all(n) != -1) return 12;
    if (and_mid(n) != -256) return 13;

    if (or_zero(w) != 0x123456) return 14;
    if (or_one(w) != 0x123457) return 15;
    if (or_mid(w) != 0x12ff56) return 16;
    if (or_both(w) != 0x12ffff) return 17;
    if (or_hi(w) != -52138) return 18;

    if (xor_zero(w) != 0x123456) return 19;
    if (xor_low(w) != 0x1234a9) return 20;
    if (xor_mid(w) != 0x12cb56) return 21;
    if (xor_hi(w) != -1231786) return 22;

    if (uand(0x123456u) != 6) return 23;
    if (ushr_after(0x123456u) != 0x12) return 24;

    /* The low-byte mask clears what is above it, so a value with rubbish up
     * there comes back clean. */
    if (and_low(0x7fffff) != 0x20) return 25;
    if (and_one(0x7fffff) != 1) return 26;

    return 42;
}
