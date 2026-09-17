/* The branch-heavy benchmark input.
 *
 * big.c has no `if` and no `while` in it, so for a while the benchmark
 * could not see the control-flow code at all -- the feature was measured
 * as costing nothing on a program that never used it. Here most of the
 * work is conditions, jumps and loops, at about the same size as big.c so
 * the two numbers can be read side by side.
 *
 * Committed rather than generated at run time, so a number from today can
 * be compared against a number from last week. Returns 42.
 */

int step0(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step1(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step2(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step3(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step4(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step5(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step6(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step7(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step8(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step9(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step10(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step11(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step12(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step13(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step14(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step15(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step16(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step17(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step18(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step19(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step20(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step21(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step22(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step23(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step24(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step25(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step26(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step27(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step28(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step29(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step30(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step31(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step32(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step33(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step34(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step35(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step36(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step37(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step38(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step39(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step40(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step41(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step42(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step43(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step44(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step45(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step46(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step47(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step48(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step49(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step50(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step51(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step52(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step53(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step54(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int step55(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 1;
    else
        return 0;
}

int step56(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 2;
    else
        return 0;
}

int step57(int a, int b) {
    int t = 0;

    while (a) {
        if (b)
            t = t + 1;
        else if (a)
            t = t + 2;
        else
            t = t - 1;
        a = a - 1;
    }
    if (t)
        return t - 0;
    else
        return 0;
}

int main(void) {
    int s = 0;
    int n = 0;
    s = s + step0(2, 1);
    s = s + step1(2, 1);
    s = s + step2(2, 1);
    s = s + step3(2, 1);
    s = s + step4(2, 1);
    s = s + step5(2, 1);
    s = s + step6(2, 1);
    s = s + step7(2, 1);
    s = s + step8(2, 1);
    s = s + step9(2, 1);
    s = s + step10(2, 1);
    s = s + step11(2, 1);
    s = s + step12(2, 1);
    s = s + step13(2, 1);
    s = s + step14(2, 1);
    s = s + step15(2, 1);
    s = s + step16(2, 1);
    s = s + step17(2, 1);
    s = s + step18(2, 1);
    s = s + step19(2, 1);
    s = s + step20(2, 1);
    s = s + step21(2, 1);
    s = s + step22(2, 1);
    s = s + step23(2, 1);
    s = s + step24(2, 1);
    s = s + step25(2, 1);
    s = s + step26(2, 1);
    s = s + step27(2, 1);
    s = s + step28(2, 1);
    s = s + step29(2, 1);
    s = s + step30(2, 1);
    s = s + step31(2, 1);
    s = s + step32(2, 1);
    s = s + step33(2, 1);
    s = s + step34(2, 1);
    s = s + step35(2, 1);
    s = s + step36(2, 1);
    s = s + step37(2, 1);
    s = s + step38(2, 1);
    s = s + step39(2, 1);
    s = s + step40(2, 1);
    s = s + step41(2, 1);
    s = s + step42(2, 1);
    s = s + step43(2, 1);
    s = s + step44(2, 1);
    s = s + step45(2, 1);
    s = s + step46(2, 1);
    s = s + step47(2, 1);
    s = s + step48(2, 1);
    s = s + step49(2, 1);
    s = s + step50(2, 1);
    s = s + step51(2, 1);
    s = s + step52(2, 1);
    s = s + step53(2, 1);
    s = s + step54(2, 1);
    s = s + step55(2, 1);
    s = s + step56(2, 1);
    s = s + step57(2, 1);

    /* Each step returns 2 - (i %% 3), so the sum is a known number; the
       loop below counts it down and the last line lands it on 42. */
    while (s) {
        n = n + 1;
        s = s - 1;
    }

    n = n - 17;

    return n;
}
