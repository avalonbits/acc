/* The benchmark input for the integer types.
 *
 * The other three are written entirely in int, so the widening on every
 * narrow load, the truncation on every narrow store and the unsigned
 * comparisons were all invisible to the benchmark when they were added.
 * Here every width appears, and they are mixed inside expressions so that
 * the conversions between them are emitted rather than only the loads.
 *
 * Committed rather than generated at run time. About the same size as the
 * other three, so the four numbers can be read side by side. Returns 42.
 */

int step0(signed char a, unsigned char b, short c, unsigned short d) {
    char small = a + 0;
    unsigned char usmall = b + 0;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    signed int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 0;
}

int step1(signed char a, unsigned char b, signed short int c, unsigned short int d) {
    char small = a + 1;
    unsigned char usmall = b + 1;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    signed int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 1;
}

int step2(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 2;
    unsigned char usmall = b + 2;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    signed int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 2;
}

int step3(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 3;
    unsigned char usmall = b + 3;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    signed int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 0;
}

int step4(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 4;
    unsigned char usmall = b + 4;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    signed int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 1;
}

int step5(signed char a, unsigned char b, signed short int c, unsigned short int d) {
    char small = a + 5;
    unsigned char usmall = b + 0;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    signed int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 2;
}

int step6(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 6;
    unsigned char usmall = b + 1;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    signed int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 0;
}

int step7(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 0;
    unsigned char usmall = b + 2;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 1;
}

int step8(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 1;
    unsigned char usmall = b + 3;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    signed int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 2;
}

int step9(signed char a, unsigned char b, signed short int c, unsigned short int d) {
    char small = a + 2;
    unsigned char usmall = b + 4;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 0;
}

int step10(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 3;
    unsigned char usmall = b + 0;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 1;
}

int step11(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 4;
    unsigned char usmall = b + 1;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 2;
}

int step12(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 5;
    unsigned char usmall = b + 2;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 0;
}

int step13(signed char a, unsigned char b, signed short int c, unsigned short int d) {
    char small = a + 6;
    unsigned char usmall = b + 3;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 1;
}

int step14(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 0;
    unsigned char usmall = b + 4;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 2;
}

int step15(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 1;
    unsigned char usmall = b + 0;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 0;
}

int step16(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 2;
    unsigned char usmall = b + 1;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 1;
}

int step17(signed char a, unsigned char b, signed short int c, unsigned short int d) {
    char small = a + 3;
    unsigned char usmall = b + 2;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 2;
}

int step18(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 4;
    unsigned char usmall = b + 3;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 0;
}

int step19(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 5;
    unsigned char usmall = b + 4;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 1;
}

int step20(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 6;
    unsigned char usmall = b + 0;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 2;
}

int step21(signed char a, unsigned char b, signed short int c, unsigned short int d) {
    char small = a + 0;
    unsigned char usmall = b + 1;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 0;
}

int step22(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 1;
    unsigned char usmall = b + 2;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 1;
}

int step23(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 2;
    unsigned char usmall = b + 3;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 2;
}

int step24(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 3;
    unsigned char usmall = b + 4;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 0;
}

int step25(signed char a, unsigned char b, signed short int c, unsigned short int d) {
    char small = a + 4;
    unsigned char usmall = b + 0;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 1;
}

int step26(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 5;
    unsigned char usmall = b + 1;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 2;
}

int step27(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 6;
    unsigned char usmall = b + 2;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 0;
}

int step28(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 0;
    unsigned char usmall = b + 3;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 1;
}

int step29(signed char a, unsigned char b, signed short int c, unsigned short int d) {
    char small = a + 1;
    unsigned char usmall = b + 4;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 2;
}

int step30(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 2;
    unsigned char usmall = b + 0;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 0;
}

int step31(char a, unsigned char b, short c, unsigned short d) {
    char small = a + 3;
    unsigned char usmall = b + 1;
    short mid = c + d;
    unsigned short umid = d - c;
    unsigned int wide = umid + usmall;
    int total = small + mid;

    while (small > 0) {
        if (usmall < b)
            total = total + 1;
        else if (wide > umid)
            total = total - 1;
        small = small - 1;
    }
    if (total != 0)
        total = total - total;

    return total + 1;
}

int main(void) {
    char a = 3;
    unsigned char b = 200;
    short c = 1000;
    unsigned short d = 60000;
    int s = 0;
    int n = 0;
    s = s + step0(a, b, c, d);
    s = s + step1(a, b, c, d);
    s = s + step2(a, b, c, d);
    s = s + step3(a, b, c, d);
    s = s + step4(a, b, c, d);
    s = s + step5(a, b, c, d);
    s = s + step6(a, b, c, d);
    s = s + step7(a, b, c, d);
    s = s + step8(a, b, c, d);
    s = s + step9(a, b, c, d);
    s = s + step10(a, b, c, d);
    s = s + step11(a, b, c, d);
    s = s + step12(a, b, c, d);
    s = s + step13(a, b, c, d);
    s = s + step14(a, b, c, d);
    s = s + step15(a, b, c, d);
    s = s + step16(a, b, c, d);
    s = s + step17(a, b, c, d);
    s = s + step18(a, b, c, d);
    s = s + step19(a, b, c, d);
    s = s + step20(a, b, c, d);
    s = s + step21(a, b, c, d);
    s = s + step22(a, b, c, d);
    s = s + step23(a, b, c, d);
    s = s + step24(a, b, c, d);
    s = s + step25(a, b, c, d);
    s = s + step26(a, b, c, d);
    s = s + step27(a, b, c, d);
    s = s + step28(a, b, c, d);
    s = s + step29(a, b, c, d);
    s = s + step30(a, b, c, d);
    s = s + step31(a, b, c, d);

    while (s > 0) {
        n = n + 1;
        s = s - 1;
    }

    n = n - -11;

    return n;
}
