/* Whether a value is true, when the only bits it has are in the third byte.
 *
 * An int here is twenty-four bits and the upper byte of HL has no name, so
 * asking "is this zero" cannot be the 16-bit `ld a,l` then `or a,h`: that
 * tests two thirds of the value and calls 0x010000 false. acc tries it
 * anyway, because when it says the low two bytes hold something the answer
 * is settled -- and only when they are empty does it go on and subtract zero
 * from the whole width.
 *
 * So the values here are the ones that tell those two paths apart: 0x010000
 * and its neighbours are true with nothing in the low two bytes, 0x00ffff is
 * true with nothing in the third, and zero is false everywhere. Both ways of
 * asking are covered, because the jumps are built differently for each: `if`
 * and `while` jump when the value is false, `do`/`while` and `||` jump when
 * it is true.
 */
static int opaque(int x) { return x; }

static int if_true(int x) { if (x) return 1; return 0; }
static int while_once(int x) { int n = 0; while (x) { n++; x = 0; } return n; }
static int not_(int x) { return !x; }
static int cond(int x) { return x ? 7 : 9; }
static int or_(int x) { return x || 0; }
static int and_(int x) { return x && 1; }

/* do/while jumps back when the condition is true, which is the other shape. */
static int do_once(int x)
{
    int n = 0;

    do { n++; if (n > 3) break; } while (x && n < 2);

    return n;
}

static int check(int x, int want)
{
    if (if_true(x) != want) return 1;
    if (while_once(x) != want) return 2;
    if (not_(x) != !want) return 3;
    if (cond(x) != (want ? 7 : 9)) return 4;
    if (or_(x) != want) return 5;
    if (and_(x) != want) return 6;

    return 0;
}

int main(void)
{
    int bad;

    /* Nothing below the third byte, so the cheap test says "empty" and the
     * answer has to come from the byte it cannot see. */
    if ((bad = check(opaque(0x010000), 1)) != 0) return 10 + bad;
    if ((bad = check(opaque(0x7f0000), 1)) != 0) return 20 + bad;
    if ((bad = check(opaque(0xff0000), 1)) != 0) return 30 + bad;

    /* Something in the low two bytes, so the cheap test settles it. */
    if ((bad = check(opaque(0x00ffff), 1)) != 0) return 40 + bad;
    if ((bad = check(opaque(1), 1)) != 0) return 50 + bad;
    if ((bad = check(opaque(0x010001), 1)) != 0) return 60 + bad;

    /* And the one value that really is false. */
    if ((bad = check(opaque(0), 0)) != 0) return 70 + bad;

    if (do_once(opaque(0x010000)) != 2) return 80;
    if (do_once(opaque(0)) != 1) return 81;

    return 42;
}
