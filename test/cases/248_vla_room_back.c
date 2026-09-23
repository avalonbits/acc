/* The room an array whose length is worked out takes, given back.
 *
 * Such an array is taken off the stack when its declaration runs, and C99
 * ends its lifetime when execution leaves its scope, so a loop whose body
 * declares one should take the room and give it back on every turn. acc
 * meant to give it back at the end of the block, at a break and at a
 * continue -- and never did any of them: "no mark taken" was -1, and was
 * asked about as `>= 0`, but the mark is a frame slot, and every frame slot
 * is negative. So every turn took the room again until the stack ran into
 * the heap.
 *
 * A goto back to a label above the declaration leaves its scope too, and
 * was not handled even in intent -- including a label in a block that has
 * already closed. It gives the room back now as well.
 * gcc's pr43220, 20040811-1 and vla-dealloc-1 are a million turns each.
 *
 * Checked two ways: equal-sized arrays on different turns have to land in
 * the same place, and one loop takes more in total than the machine has.
 */
char *seen[3];

static int loop_body(void) {
    int n;

    for (n = 0; n < 50; n++) {
        int x[n % 10 + 1];

        x[0] = n;
        if (n == 9) seen[0] = (char *) x;
        if (n == 49) seen[1] = (char *) x;
    }

    return seen[0] == seen[1];
}

static int goto_above(void) {
    int n = 0;
    char *a = 0, *b = 0;

top:;
    int x[n % 10 + 1];

    x[0] = n;
    if (n == 9) a = (char *) x;
    if (n == 49) b = (char *) x;
    n++;
    if (n < 50)
        goto top;

    return a == b;
}

static int goto_out_of_inner(void) {
    int n = 0;
    char *a = 0, *b = 0;

again:
    {
        int x[n % 10 + 1];

        x[0] = n;
        if (n == 9) a = (char *) x;
        if (n == 49) b = (char *) x;
        n++;
        if (n < 50)
            goto again;
    }

    return a == b;
}

/* The label is in a block that has closed by the time the goto runs --
 * gcc's vla-dealloc-1. The goto does not leave the label's block, but it
 * does leave the array's scope, which began after that block ended. */
static int into_closed_block(void) {
    int n = 0;
    char *a = 0, *b = 0;

    if (0) {
    inside:;
    }
    int x[n % 10 + 1];

    x[0] = n;
    if (n == 9) a = (char *) x;
    if (n == 49) b = (char *) x;
    n++;
    if (n < 50)
        goto inside;

    return a == b;
}

static int break_and_continue(void) {
    int n, kept = 0;
    char *a = 0, *b = 0;

    for (n = 0; n < 100; n++) {
        int x[n % 10 + 1];

        x[0] = n;
        if (n == 19) a = (char *) x;
        if (n == 89) b = (char *) x;
        if (n % 3 == 0)
            continue;
        if (n == 95)
            break;
        kept++;
    }

    return a == b && kept == 63;
}

/* Two thousand turns of a 1,500-byte array is three megabytes, which is
 * more than the Agon has: if any of it is kept, this does not come back. */
static int big(void) {
    int n, sum = 0;

    for (n = 0; n < 2000; n++) {
        char x[1500 + n % 7];

        x[0] = (char) n;
        x[1499] = 1;
        sum += x[1499];
    }

    return sum == 2000;
}

int main(void) {
    int r = 0;

    if (loop_body()) r++;
    if (goto_above()) r++;
    if (goto_out_of_inner()) r++;
    if (into_closed_block()) r++;
    if (break_and_continue()) r++;
    if (big()) r++;

    return r + 36;              /* 6 checks */
}
