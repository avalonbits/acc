/* Arithmetic on bytes, done at byte width.
 *
 * C promotes anything narrower than int before operating on it and the store
 * narrows the result again. Computing in eight bits gives the same answer --
 * arithmetic modulo 256 is a ring homomorphism, so truncating as you go and
 * truncating at the end agree -- but only for operators where that holds, and
 * only when nothing wider ever sees the value.
 *
 * Everything here wraps, because a byte path that quietly computed at int
 * width would pass a test whose values all fit.
 */
int main(void) {
    unsigned char a = 200;
    unsigned char b = 100;
    signed char s = 100;
    signed char t = 100;
    int r = 0;

    a = a + b;              /* 300 does not fit a byte: 44 */
    if (a == 44) r = r + 1;

    a = a - b;              /* 44 - 100 is negative: 200 */
    if (a == 200) r = r + 2;

    a = a & 60;
    if (a == 8) r = r + 4;

    a = a | 3;
    if (a == 11) r = r + 8;

    a = a ^ 255;
    if (a == 244) r = r + 16;

    a = a >> 2;             /* unsigned: zeros shift in */
    if (a == 61) r = r + 32;

    s = s + t;              /* 200 does not fit a signed char: -56 */
    if (s == -56) r = r + 64;

    s = s >> 2;             /* signed: the sign shifts down, -14 */
    if (s == -14) r = r + 128;

    t = t << 2;             /* 400 does not fit: -112 */
    if (t == -112) r = r + 256;
    /* 511 */

    return r - 469;
}
