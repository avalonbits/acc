/* The benchmark input for the operators that go through the runtime.
 *
 * * / % & | ^ << and >> have no instruction behind them on this chip, so
 * every one is a call acc has to record and patch, and the routines they
 * call are emitted into the image at the end. None of that was measured:
 * the other four inputs use none of these operators, and the coverage note
 * only started saying so once it stopped matching the `/*` of a comment.
 *
 * Committed rather than generated at run time. About the same size as the
 * others, so the numbers read side by side. Returns 42.
 */

int step0(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 2;
    int quot = a / 2;
    int rest = a % 2;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 0;
}

int step1(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 3;
    int quot = a / 3;
    int rest = a % 3;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 1;
}

int step2(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 4;
    int quot = a / 4;
    int rest = a % 4;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 2;
}

int step3(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 5;
    int quot = a / 5;
    int rest = a % 5;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 0;
}

int step4(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 6;
    int quot = a / 6;
    int rest = a % 6;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 1;
}

int step5(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 7;
    int quot = a / 2;
    int rest = a % 7;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 2;
}

int step6(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 8;
    int quot = a / 3;
    int rest = a % 8;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 0;
}

int step7(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 2;
    int quot = a / 4;
    int rest = a % 9;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 1;
}

int step8(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 3;
    int quot = a / 5;
    int rest = a % 10;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 2;
}

int step9(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 4;
    int quot = a / 6;
    int rest = a % 2;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 0;
}

int step10(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 5;
    int quot = a / 2;
    int rest = a % 3;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 1;
}

int step11(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 6;
    int quot = a / 3;
    int rest = a % 4;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 2;
}

int step12(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 7;
    int quot = a / 4;
    int rest = a % 5;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 0;
}

int step13(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 8;
    int quot = a / 5;
    int rest = a % 6;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 1;
}

int step14(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 2;
    int quot = a / 6;
    int rest = a % 7;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 2;
}

int step15(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 3;
    int quot = a / 2;
    int rest = a % 8;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 0;
}

int step16(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 4;
    int quot = a / 3;
    int rest = a % 9;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 1;
}

int step17(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 5;
    int quot = a / 4;
    int rest = a % 10;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 2;
}

int step18(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 6;
    int quot = a / 5;
    int rest = a % 2;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 0;
}

int step19(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 7;
    int quot = a / 6;
    int rest = a % 3;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 1;
}

int step20(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 8;
    int quot = a / 2;
    int rest = a % 4;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 2;
}

int step21(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 2;
    int quot = a / 3;
    int rest = a % 5;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 0;
}

int step22(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 3;
    int quot = a / 4;
    int rest = a % 6;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 1;
}

int step23(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 4;
    int quot = a / 5;
    int rest = a % 7;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 2;
}

int step24(int a, int b, unsigned int u) {
    int mask = a & 65535;
    int high = a | 16711680;
    int flip = a ^ b;
    int up = b << 3;
    int down = a >> 2;
    unsigned int udown = u >> 5;
    int prod = a * 5;
    int quot = a / 6;
    int rest = a % 8;
    int total = mask + high + flip + up + down + prod + quot + rest;

    while (total > 1000) {
        if ((total & 1) == 0)
            total = total >> 1;
        else if (total % 3 == 0)
            total = total / 3;
        else
            total = total - 1;
    }
    if (udown > 0)
        total = total ^ 0;

    return total - total + 0;
}

int main(void) {
    int s = 0;
    int n = 0;
    s = s + step0(123456, 654321, 8000000);
    s = s + step1(123456, 654321, 8000000);
    s = s + step2(123456, 654321, 8000000);
    s = s + step3(123456, 654321, 8000000);
    s = s + step4(123456, 654321, 8000000);
    s = s + step5(123456, 654321, 8000000);
    s = s + step6(123456, 654321, 8000000);
    s = s + step7(123456, 654321, 8000000);
    s = s + step8(123456, 654321, 8000000);
    s = s + step9(123456, 654321, 8000000);
    s = s + step10(123456, 654321, 8000000);
    s = s + step11(123456, 654321, 8000000);
    s = s + step12(123456, 654321, 8000000);
    s = s + step13(123456, 654321, 8000000);
    s = s + step14(123456, 654321, 8000000);
    s = s + step15(123456, 654321, 8000000);
    s = s + step16(123456, 654321, 8000000);
    s = s + step17(123456, 654321, 8000000);
    s = s + step18(123456, 654321, 8000000);
    s = s + step19(123456, 654321, 8000000);
    s = s + step20(123456, 654321, 8000000);
    s = s + step21(123456, 654321, 8000000);
    s = s + step22(123456, 654321, 8000000);
    s = s + step23(123456, 654321, 8000000);
    s = s + step24(123456, 654321, 8000000);

    while (s > 0) {
        n = n + 1;
        s = s - 1;
    }

    n = n - -18;

    return n;
}
