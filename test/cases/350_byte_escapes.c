/* Where byte arithmetic is not allowed, and has to promote.
 *
 * Computing in eight bits is only indistinguishable from C when the result is
 * narrowed straight back. Everywhere else the wider value is visible and has
 * to be there: 200 + 100 is 300 in a comparison, in an argument, in a return
 * and in a wider variable -- and 44 only when it lands in a byte.
 *
 * Every line here would be wrong if the compiler took the byte path.
 */
int widen(int n) { return n; }

/* Observes the value rather than passing it through, so that narrowing the
   argument changes the answer rather than being hidden by the store. */
int over250(int n) { return n > 250; }

int main(void) {
    unsigned char a = 200;
    unsigned char b = 100;
    unsigned char dest = 0;
    unsigned char flag = 0;
    int wide = 0;
    int r = 0;

    /* A comparison sees the sum in full. */
    if (a + b == 300) r = r + 1;
    if (a + b == 44) r = r + 1000;

    /* So does a wider variable. */
    wide = a + b;
    if (wide == 300) r = r + 2;

    /* And an argument. */
    if (widen(a + b) == 300) r = r + 4;

    /* And the middle of a chain whose end is a comparison. */
    if (a + b + b == 400) r = r + 8;

    /* But a byte destination narrows, which is the whole point. */
    dest = a + b;
    if (dest == 44) r = r + 16;

    /* A chain into a byte destination: only the last operation can be done
       narrow, and the answer is the same either way. */
    dest = a + b + b;
    if (dest == 144) r = r + 32;

    /* The two that need the destination to be narrow *and* something inside
       the expression to need the wider value. Nothing above reaches them:
       a condition and a return are never parsed inside an assignment, so
       clearing the destination there is unreachable, but these two are the
       real thing.

       A comparison feeding a byte: the sum has to be 300 to be over 250, and
       a compiler that narrowed the `+` would compare 44 and store 0. */
    flag = a + b > 250;
    if (flag == 1) r = r + 64;

    /* An argument inside an assignment to a byte. The callee has to be handed
       300, not 44, even though the answer ends up one byte wide -- so the
       callee has to look at the value rather than return it, or the store
       would hide the difference. */
    flag = over250(a + b);
    if (flag == 1) r = r + 128;
    /* 255 */

    return r - 213;
}
