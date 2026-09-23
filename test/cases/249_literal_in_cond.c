/* A compound literal inside the third operand of a `?:`.
 *
 * The middle operand's answer waits for the join in a slot of the frame's
 * scratch area that nothing on the value stack points at, so the `?:` locks
 * it while the third operand is compiled. A compound literal's initialiser
 * ended each of its values the way a statement ends, which freed the whole
 * scratch area, lock and all, and the inner `?:` below was then handed the
 * outer one's slot: it built its long long over the answer it was waiting
 * for, and the second and third ways through came out wrong. */
static long long f8(void) { return 800000000000LL; }
static long f4(void) { return 40000L; }
static int f3(void) { return 3; }

struct P { int a, b; };

int main(void) {
    int r = 0, n;
    long long v;

    n = 1;
    v = n > 1 ? f8() : (int){ n } ? (long long) f4() : (long long) f3();
    if (v == 40000LL) r++;
    n = 2;
    v = n > 1 ? f8() : (int){ n } ? (long long) f4() : (long long) f3();
    if (v == 800000000000LL) r++;
    n = 0;
    v = n > 1 ? f8() : (int){ n } ? (long long) f4() : (long long) f3();
    if (v == 3LL) r++;

    /* The same, with the literal a struct and then an array. */
    n = 1;
    v = n > 1 ? f8() : (struct P){ n, 0 }.a ? (long long) f4()
                                              : (long long) f3();
    if (v == 40000LL) r++;
    n = 0;
    v = n > 1 ? f8() : (int[]){ n, 1 }[0] ? (long long) f4()
                                            : (long long) f3();
    if (v == 3LL) r++;

    return r + 37;              /* 5 checks */
}
