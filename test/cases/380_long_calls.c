/* Longs across a call, where the ABI differs from everything else.
 *
 * A long argument takes two slots -- six bytes for a four-byte value -- and a
 * long result comes back in HL with its high byte in E. Both are what agondev
 * does, and test/abi.sh pins them.
 *
 * It also needs the call site to know what the parameter was declared as: an
 * int argument passed to a long parameter has to be widened to four bytes and
 * given two slots, or every argument after it lands in the wrong place. That
 * is why acc records each function's parameter types.
 */
long add(long a, long b) { return a + b; }
int take(long a, int b) { return a - 999958 + b - 7; }
int mixed(int a, long b, int c) { return b - 1999958 + a + c - 9; }
long doubled(long n) { return n + n; }

/* An address and a long in one call. The long is pushed through HL, six
 * bytes at a time, and the address was sitting in HL: it has to come out
 * before the push, or the callee is handed whatever the long left there. */
struct held { int id; long big; };

int both(struct held *h, long big) { return h->id == 4 && h->big == big; }

int main(void) {
    long sum = add(1000000, 2000000);
    int r = 0;

    if (sum == 3000000) r = r + 1;

    /* An int constant handed to a long parameter: widened at the call. */
    if (take(1000000, 7) == 42) r = r + 2;
    if (mixed(4, 2000000, 5) == 42) r = r + 4;

    /* A long result used straight away, and nested. */
    if (doubled(1000000) == 2000000) r = r + 8;
    if (doubled(doubled(500000)) == 2000000) r = r + 16;

    {
        struct held one;
        long v = -5;

        one.id = 4;
        one.big = -5;

        /* The long argument is a variable: it goes to the callee out of the
         * frame, and nothing before the push has had to touch HL. The
         * address is in HL, and this is where it was lost. */
        if (both(&one, v)) r = r + 32;
        if (both(&one, -5)) r = r + 64;
    }
    /* 127 */

    return r - 85;
}
