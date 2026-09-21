/* Adding or subtracting a little, one instruction at a time.
 *
 * `inc hl` is one byte and one cycle; putting the same amount in a register
 * and adding it is five of each, because an immediate here is the full width
 * of a register. So up to four steps acc writes the steps, and past that it
 * goes back to loading the amount -- and the constants below sit on both
 * sides of that edge.
 *
 * The step also leaves the flags alone where an add would have set the carry,
 * so each result is used somewhere the flags would be read if anything still
 * believed them: straight into a branch, and into a comparison that follows.
 *
 * An int here is twenty-four bits, so stepping off either end wraps, and the
 * ends are checked as well as the middle.
 */
static int id(int x) { return x; }
static unsigned uid(unsigned x) { return x; }

static int p1(int x) { return x + 1; }
static int p2(int x) { return x + 2; }
static int p3(int x) { return x + 3; }
static int p4(int x) { return x + 4; }
static int p5(int x) { return x + 5; }
static int m1(int x) { return x - 1; }
static int m2(int x) { return x - 2; }
static int m4(int x) { return x - 4; }
static int m5(int x) { return x - 5; }
static int m9(int x) { return x - 9; }

static unsigned up1(unsigned x) { return x + 1u; }
static unsigned um1(unsigned x) { return x - 1u; }

/* The result goes straight into a branch, where a carry left by an add would
 * have been available and a step does not leave one. */
static int stepped_then_branched(int x) { if (x + 1) return 5; return 6; }
static int stepped_then_compared(int x) { return (x + 2) < 100; }

struct wide { char pad[13]; };
static struct wide table[4];

int main(void)
{
    int x = id(10);
    char buf[8];
    char *b = buf;
    int *ip = &x;

    if (p1(x) != 11) return 1;
    if (p2(x) != 12) return 2;
    if (p3(x) != 13) return 3;
    if (p4(x) != 14) return 4;
    if (p5(x) != 15) return 5;
    if (m1(x) != 9) return 6;
    if (m2(x) != 8) return 7;
    if (m4(x) != 6) return 8;
    if (m5(x) != 5) return 9;
    if (m9(x) != 1) return 10;

    /* Off the ends, where the step has to wrap the way an add would.
     * Unsigned only: a signed int stepping past its largest value is
     * undefined, and agondev's optimiser takes that as a promise it will not
     * happen while acc simply wraps, so the two disagree and neither is
     * wrong. Twenty-four bits is twenty-four bits either way round. */
    if (up1(uid(16777215u)) != 0u) return 13;
    if (um1(uid(0u)) != 16777215u) return 14;
    if (up1(uid(8388607u)) != 8388608u) return 11;
    if (um1(uid(8388608u)) != 8388607u) return 12;

    if (stepped_then_branched(id(0)) != 5) return 15;
    if (stepped_then_branched(id(-1)) != 6) return 16;
    if (stepped_then_compared(id(50)) != 1) return 17;
    if (stepped_then_compared(id(200)) != 0) return 18;

    /* Pointer steps: one byte a step, and a struct wide enough that the
     * amount is not small any more. */
    if ((b + 1) - buf != 1) return 19;
    if ((b + 4) - buf != 4) return 20;
    if ((b + 5) - buf != 5) return 21;
    if ((ip + 1) - ip != 1) return 22;
    if ((table + 2) - table != 2) return 23;
    if ((char *) (table + 1) - (char *) table != 13) return 24;

    return 42;
}
