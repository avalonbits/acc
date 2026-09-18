/* `x op= y`, for all ten operators and every kind of x.
 *
 * It means `x = x op y` with x evaluated once, which for a local is no
 * restriction at all and for `*p op= y` means the address is worked out once
 * and used twice -- to read, and then to write.
 *
 * The right side is a whole operand of op, so it is not truncated to x's
 * width before op sees it: `c += 200 + 100` adds 300, which a char then keeps
 * the low byte of, and not 44. The operator itself may still run at x's
 * width, exactly as `c = c + y` may, because its result goes straight into x
 * and nothing wider ever sees it.
 */
int main(void) {
    int a = 100;
    unsigned char c = 10;
    long n = 1000000;
    float f = 10.0;
    int target = 5;
    int *p = &target;
    int r = 0;

    a += 20;
    if (a == 120) r = r + 1;
    a -= 6;
    if (a == 114) r = r + 1;
    a *= 3;
    if (a == 342) r = r + 1;
    a /= 2;
    if (a == 171) r = r + 1;
    a %= 100;
    if (a == 71) r = r + 1;
    a &= 60;
    if (a == 4) r = r + 1;
    a |= 3;
    if (a == 7) r = r + 1;
    a ^= 5;
    if (a == 2) r = r + 1;
    a <<= 4;
    if (a == 32) r = r + 1;
    a >>= 3;
    if (a == 4) r = r + 1;

    /* The right side whole, then the byte kept: 10 + 300 is 310, and an
     * unsigned char keeps 54 of it. */
    c += 200 + 100;
    if (c == 54) r = r + 1;

    n *= 100;
    if (n == 100000000) r = r + 1;
    f *= 4.5;
    if (f == 45.0) r = r + 1;

    /* Through a pointer, and a pointer itself. */
    *p += 37;
    if (target == 42) r = r + 1;
    *p -= 2;
    if (target == 40) r = r + 1;
    p += 0;
    if (*p == 40) r = r + 1;

    /* An assignment has the value it stored, and chains to the right. */
    if ((a += 1) == 5) r = r + 1;
    target = 1;
    a = 2;
    a += target += 1;
    if (a == 4) r = r + 1;

    /* 18 */
    return r + 24;
}
