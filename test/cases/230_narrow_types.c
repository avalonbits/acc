/* char, short and their unsigned forms: declared, stored, loaded and compared.
 *
 * The widths are agondev's, so both compilers must agree about where each one
 * wraps: char at 128, short at 32768, and their unsigned forms at 256 and
 * 65536. */
int main(void) {
    char c = 100;
    unsigned char uc = 200;
    short s = 30000;
    unsigned short us = 60000;
    int r = 0;

    if (c == 100) r = r + 1;
    if (uc == 200) r = r + 2;
    if (s == 30000) r = r + 4;
    if (us == 60000) r = r + 8;

    c = c + 100;            /* 200 does not fit a signed char: -56 */
    if (c == -56) r = r + 16;

    uc = uc + 100;          /* 300 does not fit an unsigned char: 44 */
    if (uc == 44) r = r + 32;

    s = s + 10000;          /* 40000 does not fit a signed short: -25536 */
    if (s == -25536) r = r + 64;

    us = us + 10000;        /* 70000 does not fit an unsigned short: 4464 */
    if (us == 4464) r = r + 128;
    /* 255 */

    return r - 213;
}
