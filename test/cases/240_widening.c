/* A narrow value widens to int when it is used, and how it widens depends on
 * its sign. The same byte pattern means -1 as a char and 255 as an unsigned
 * char, and every use has to agree with the declaration rather than with the
 * bits. */
int as_int(int n) { return n; }

int main(void) {
    char c = -1;
    unsigned char uc = 255;
    short s = -1;
    unsigned short us = 65535;
    int r = 0;

    if (as_int(c) == -1) r = r + 1;
    if (as_int(uc) == 255) r = r + 2;
    if (as_int(s) == -1) r = r + 4;
    if (as_int(us) == 65535) r = r + 8;

    /* Arithmetic happens at int width, so the sum of two chars that would
       overflow a char does not wrap until it is stored back into one. */
    if (c + c == -2) r = r + 16;
    if (uc + uc == 510) r = r + 32;

    /* And in a condition, where the whole widened value has to be tested:
       an unsigned short of 65280 has a zero low byte. */
    us = 65280;
    if (us) r = r + 64;
    /* 127 */

    return r - 85;
}
