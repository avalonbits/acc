/* A one-byte result comes back in A, which is where agondev puts one.
 *
 * That is the one place the two conventions genuinely differ: every other
 * width comes back in HL, and a narrow argument is a narrow read of a full
 * slot, but A is a different register. test/abi.sh pins agondev's side of it.
 *
 * Both signednesses, because the widening on the caller's side has to agree
 * with the declaration rather than with the bits: the same byte is -1 from a
 * char and 255 from an unsigned char.
 */
/* A constant return, which is the case that needs the byte put in A
   deliberately: returning a variable converts it on the way out and leaves it
   in A as a side effect, so a compiler that forgot the store would still look
   right everywhere except here. */
char literal(void) { return 90; }
unsigned char uliteral(void) { return 200; }

char small(int n) { return n; }
unsigned char usmall(int n) { return n; }
short middling(int n) { return n; }

int main(void) {
    int r = 0;

    if (small(300) == 44) r = r + 1;        /* 300 does not fit: 44 */
    if (small(-1) == -1) r = r + 2;
    if (small(255) == -1) r = r + 4;        /* 255 as a signed char */
    if (usmall(255) == 255) r = r + 8;
    if (usmall(-1) == 255) r = r + 16;
    if (middling(100000) == -31072) r = r + 32;
    if (literal() == 90) r = r + 64;
    if (uliteral() == 200) r = r + 128;
    /* 255 */

    /* The result used as a value rather than only compared. */
    r = r + small(2) + usmall(3);
    /* 255 + 5 = 260 */

    return r - 218;
}
