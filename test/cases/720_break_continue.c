/* break and continue in every loop, and in a switch inside a loop.
 *
 * A break leaves the innermost loop or switch; a continue goes on with the
 * innermost loop -- to its test in a while or a do, and to its step in a
 * for, so the step is not skipped. A switch passes a continue through to the
 * loop around it.
 */
int main(void) {
    int r = 0;
    int i;
    int n;

    /* while: stop at 5, and skip the evens. */
    n = 0;
    i = 0;
    while (i < 100) {
        i++;
        if (i % 2 == 0)
            continue;
        if (i > 9)
            break;
        n += i;
    }
    if (n == 25 && i == 11) r = r + 1;

    /* for: continue still runs the step. */
    n = 0;
    for (i = 0; i < 10; i++) {
        if (i == 3 || i == 6)
            continue;
        n += i;
    }
    if (n == 36 && i == 10) r = r + 1;

    /* for (;;) left only by break. */
    n = 0;
    for (;;) {
        n++;
        if (n == 7)
            break;
    }
    if (n == 7) r = r + 1;

    /* do: continue goes to the test, which still decides. */
    n = 0;
    i = 0;
    do {
        i++;
        if (i == 2)
            continue;
        n += i;
    } while (i < 5);
    if (n == 13) r = r + 1;

    /* Nested: break and continue act on the inner loop only. */
    n = 0;
    for (int a = 0; a < 4; a++) {
        for (int b = 0; b < 4; b++) {
            if (b == 2)
                break;
            if (a == 1)
                continue;
            n += 10 * a + b;
        }
    }
    if (n == 103) r = r + 1;

    /* A break in a switch leaves the switch; a continue in one goes on with
     * the loop around it. */
    n = 0;
    for (i = 0; i < 6; i++) {
        switch (i) {
        case 1:
            continue;
        case 3:
            break;
        default:
            n += i;
            break;
        }
        n += 100;
    }
    if (n == 511) r = r + 1;

    /* 6 */
    return r + 36;
}
