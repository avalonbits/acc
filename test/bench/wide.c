/* The benchmark input for the four-byte types.
 *
 * A long is wider than a register, so it lives in the frame and every
 * operation on it is a helper taking two addresses. None of the other six
 * inputs has one, so the whole of that path went in unmeasured -- and the
 * coverage note did not say so, because the word "long" in a comment in
 * big.c counted as using the keyword until the note learnt to strip them.
 *
 * float and double are here for what works of them: a literal, a copy, and
 * passing and returning one. Their arithmetic is not implemented yet.
 *
 * Committed rather than generated at run time. Returns 42.
 */
float echo_float(float x) { return x; }


long step0(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 0.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 0;
}

long step1(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 1.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 1;
}

long step2(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 2.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 2;
}

long step3(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 3.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 0;
}

long step4(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 4.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 1;
}

long step5(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 5.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 2;
}

long step6(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 6.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 0;
}

long step7(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 7.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 1;
}

long step8(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 8.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 2;
}

long step9(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 0.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 0;
}

long step10(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 1.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 1;
}

long step11(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 2.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 2;
}

long step12(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 3.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 0;
}

long step13(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 4.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 1;
}

long step14(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 5.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 2;
}

long step15(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 6.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 0;
}

long step16(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 7.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 1;
}

long step17(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 8.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 2;
}

long step18(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 0.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 0;
}

long step19(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 1.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 1;
}

long step20(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 2.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 2;
}

long step21(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 3.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 0;
}

long step22(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 4.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 1;
}

long step23(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 5.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 2;
}

long step24(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 6.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 0;
}

long step25(long a, long b, unsigned long u) {
    long sum = a + b;
    long diff = b - a;
    long mask = a & 16777215;
    long high = b | 255;
    long flip = a ^ b;
    unsigned long wide = u - 1000;
    double f = 7.5;
    float g = echo_float(f);
    long total = sum + diff;

    while (total > 1000000) {
        if (mask < diff)
            total = total - diff;
        else if (wide > u)
            total = total - 1;
        else
            total = total - mask;
    }
    if (high != flip)
        total = total - total;

    g = f;
    f = g;

    return total + 1;
}

int main(void) {
    long s = 0;
    int n = 0;
    s = s + step0(1000000, 3000000, 2000000);
    s = s + step1(1000000, 3000000, 2000000);
    s = s + step2(1000000, 3000000, 2000000);
    s = s + step3(1000000, 3000000, 2000000);
    s = s + step4(1000000, 3000000, 2000000);
    s = s + step5(1000000, 3000000, 2000000);
    s = s + step6(1000000, 3000000, 2000000);
    s = s + step7(1000000, 3000000, 2000000);
    s = s + step8(1000000, 3000000, 2000000);
    s = s + step9(1000000, 3000000, 2000000);
    s = s + step10(1000000, 3000000, 2000000);
    s = s + step11(1000000, 3000000, 2000000);
    s = s + step12(1000000, 3000000, 2000000);
    s = s + step13(1000000, 3000000, 2000000);
    s = s + step14(1000000, 3000000, 2000000);
    s = s + step15(1000000, 3000000, 2000000);
    s = s + step16(1000000, 3000000, 2000000);
    s = s + step17(1000000, 3000000, 2000000);
    s = s + step18(1000000, 3000000, 2000000);
    s = s + step19(1000000, 3000000, 2000000);
    s = s + step20(1000000, 3000000, 2000000);
    s = s + step21(1000000, 3000000, 2000000);
    s = s + step22(1000000, 3000000, 2000000);
    s = s + step23(1000000, 3000000, 2000000);
    s = s + step24(1000000, 3000000, 2000000);
    s = s + step25(1000000, 3000000, 2000000);

    while (s > 0) {
        n = n + 1;
        s = s - 1;
    }

    n = n - -17;

    return n;
}
