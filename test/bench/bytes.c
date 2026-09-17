/* The benchmark input for byte-width arithmetic.
 *
 * Every statement here assigns a char expression back to a char, which is
 * the shape where C's promote-then-truncate is indistinguishable from
 * computing in eight bits -- so it is the shape a compiler can keep in A.
 *
 * types.c deliberately assigns across widths, which is the case that cannot
 * use eight-bit arithmetic, so it would not show this at all.
 *
 * Committed rather than generated at run time. About the same size as the
 * others. Returns 42.
 */

int step0(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step1(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step2(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step3(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step4(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step5(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step6(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step7(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step8(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step9(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step10(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step11(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step12(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step13(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step14(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step15(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step16(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step17(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step18(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step19(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step20(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step21(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step22(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step23(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step24(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step25(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step26(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step27(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step28(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step29(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step30(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step31(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step32(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step33(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step34(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step35(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step36(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step37(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step38(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step39(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step40(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step41(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step42(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step43(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step44(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step45(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step46(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step47(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step48(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step49(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step50(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int step51(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 0;
}

int step52(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 1;
}

int step53(unsigned char a, unsigned char b) {
    unsigned char t = a;
    unsigned char u = b;

    t = t + b;
    t = t & 15;
    t = t | 128;
    t = t ^ b;
    t = t >> 2;
    u = u - a;
    u = u & 63;
    u = u | 1;
    u = u ^ t;
    u = u << 1;
    t = t + u;
    t = t & 7;

    return t - t + 2;
}

int main(void) {
    int s = 0;
    int n = 0;
    s = s + step0(200, 100);
    s = s + step1(200, 100);
    s = s + step2(200, 100);
    s = s + step3(200, 100);
    s = s + step4(200, 100);
    s = s + step5(200, 100);
    s = s + step6(200, 100);
    s = s + step7(200, 100);
    s = s + step8(200, 100);
    s = s + step9(200, 100);
    s = s + step10(200, 100);
    s = s + step11(200, 100);
    s = s + step12(200, 100);
    s = s + step13(200, 100);
    s = s + step14(200, 100);
    s = s + step15(200, 100);
    s = s + step16(200, 100);
    s = s + step17(200, 100);
    s = s + step18(200, 100);
    s = s + step19(200, 100);
    s = s + step20(200, 100);
    s = s + step21(200, 100);
    s = s + step22(200, 100);
    s = s + step23(200, 100);
    s = s + step24(200, 100);
    s = s + step25(200, 100);
    s = s + step26(200, 100);
    s = s + step27(200, 100);
    s = s + step28(200, 100);
    s = s + step29(200, 100);
    s = s + step30(200, 100);
    s = s + step31(200, 100);
    s = s + step32(200, 100);
    s = s + step33(200, 100);
    s = s + step34(200, 100);
    s = s + step35(200, 100);
    s = s + step36(200, 100);
    s = s + step37(200, 100);
    s = s + step38(200, 100);
    s = s + step39(200, 100);
    s = s + step40(200, 100);
    s = s + step41(200, 100);
    s = s + step42(200, 100);
    s = s + step43(200, 100);
    s = s + step44(200, 100);
    s = s + step45(200, 100);
    s = s + step46(200, 100);
    s = s + step47(200, 100);
    s = s + step48(200, 100);
    s = s + step49(200, 100);
    s = s + step50(200, 100);
    s = s + step51(200, 100);
    s = s + step52(200, 100);
    s = s + step53(200, 100);

    while (s > 0) {
        n = n + 1;
        s = s - 1;
    }

    n = n - 12;

    return n;
}
