/* A float negated while an earlier answer is still in a register.
 *
 * The negation took a scratch slot for the float and only then spilled
 * the register, and the spill went where the stack said the scratch was
 * free: over the slot just taken, which the stack did not yet know about.
 * So in f(a > b, -a < b) the first answer came back as the top of the
 * negated float, -32768. A slot is now reserved from when it is handed out
 * until the stack points at it. */
static int pair(int x, int y) { return x * 10 + y; }

volatile float one = 1.0f, two = 2.0f;

int main(void) {
    int r = 0;
    long l = 5;
    volatile long vl = 7;

    if (pair(two > one, -two < one) == 11) r++;
    if (pair(one < two, -one > -two) == 11) r++;
    if (pair(two == two, -(two * one) < 0) == 11) r++;
    if (pair(two > one, (float) -vl < one) == 11) r++;
    if (pair(l > 3, -two < one) == 11) r++;

    return r + 37;              /* 5 checks */
}
