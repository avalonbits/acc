/* The integer types, and where each one wraps.
 *
 * char is one byte, short is two, int is three -- the same widths agondev
 * uses, so a program compiled by either comes out the same. Arithmetic
 * happens at int width and only a store narrows. Prints 00002A -- 42. */
int main(void) {
    char c = 100;
    unsigned char uc = 200;
    short s = 30000;
    unsigned int big = 16777215;    /* every bit of a 24-bit int */
    int r = 0;

    c = c + 100;        /* 200 will not fit a signed char: -56 */
    if (c == -56) r = r + 1;

    uc = uc + 100;      /* 300 will not fit an unsigned char: 44 */
    if (uc == 44) r = r + 2;

    s = s + 10000;      /* 40000 will not fit a signed short: -25536 */
    if (s == -25536) r = r + 4;

    /* An unsigned comparison is not the signed one: as a signed int, that
       same value is -1, and -1 is not greater than 1. */
    if (big > 1) r = r + 8;

    return r + 27;
}
