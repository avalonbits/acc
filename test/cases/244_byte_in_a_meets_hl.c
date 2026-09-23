/* A byte result in A, widened while something else is living in HL.
 *
 * When the destination of an expression is a char, acc works its last step
 * out a byte at a time in A. A binary operator then wants its left operand
 * in HL and its right one somewhere else -- and a right operand still in A
 * is widened through HL to get anywhere. The allocator made room in HL by
 * moving the left operand to DE, which kept it alive, but the operator had
 * already been told it was in HL and never looked again. So:
 *
 *   x = 1 | x << 1    the OR was handed x << 1 twice, and lost the 1
 *   --w > 0           the comparison subtracted w from itself
 *
 * gcc's doloop-1 counts down an unsigned char from 0 with `while (--z > 0)`
 * and stopped after one pass instead of 256; pr109778 rotates a byte with
 * `x = x >> 4 | x << 4`. Both widenings now leave HL's owner where it is.
 */
static unsigned char id(unsigned char v) { return v; }

int main(void) {
    int r = 0, n = 0;
    unsigned char x, w, z;

    /* A constant on the left of a byte-wide shift. */
    x = id(90);  x = 1 | x << 1;       if (x == 181) r++;
    x = id(10);  x = 1 | x << 1;       if (x == 21) r++;
    x = id(90);  x = x >> 4 | x << 4;  if (x == 0xa5) r++;
    x = id(3);   x = 0x80 ^ x << 2;    if (x == 0x8c) r++;

    /* The value of a decrement, compared as it stands. */
    w = id(0);   if (--w > 0) r++;
    w = id(0);   if ((w -= 1) > 0 && w == 255) r++;

    /* And the loop gcc's test is. */
    z = id(0);
    do n++; while (--z > 0);
    if (n == 256) r++;

    /* Nothing to widen past: still right. */
    x = id(10);  x = (x << 1) | 1;     if (x == 21) r++;

    return r + 34;              /* 8 checks */
}
