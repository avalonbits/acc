/* A jump back over jumps that have been shortened.
 *
 * Shortening a jump takes two bytes out of the middle of its operand, which
 * moves everything after it: where every later jump is, and where every jump
 * goes. acc works both out by walking the jumps and the runs it is taking
 * out together in rising order, keeping a running total of what has gone.
 * A jump that goes BACKWARDS has to take that total back down again, over
 * the runs lying between it and its target -- and that is the half of the
 * arithmetic no straight-line function reaches.
 *
 * So: a loop whose body holds `if`s that shorten, and the back edge jumping
 * over all of them. Left undone, the back edge lands two bytes further on
 * for every `if` that shrank, and the loop does not return to its top.
 *
 * A ladder of body sizes rather than one loop, for the reason 216 is a
 * ladder: the name of the output file is written into the image, so the same
 * program compiled to two different names lays out differently and shortens
 * a different set of jumps. One body length can therefore be right by luck.
 * Twelve of them, each with a different number of runs to walk back over,
 * cannot all be.
 */
static int loop1(int limit)
{
    int i, n = 0;

    for (i = 0; i < limit; i++) { if (i == 0) n += 1; }

    return n;
}

static int loop2(int limit)
{
    int i, n = 0;

    for (i = 0; i < limit; i++) { if (i == 0) n += 1; if (i == 1) n += 2; }

    return n;
}

static int loop3(int limit)
{
    int i, n = 0;

    for (i = 0; i < limit; i++) { if (i == 0) n += 1; if (i == 1) n += 2; if (i == 2) n += 4; }

    return n;
}

static int loop4(int limit)
{
    int i, n = 0;

    for (i = 0; i < limit; i++) { if (i == 0) n += 1; if (i == 1) n += 2; if (i == 2) n += 4; if (i == 3) n += 8; }

    return n;
}

static int loop5(int limit)
{
    int i, n = 0;

    for (i = 0; i < limit; i++) { if (i == 0) n += 1; if (i == 1) n += 2; if (i == 2) n += 4; if (i == 3) n += 8; if (i == 4) n += 16; }

    return n;
}

static int loop6(int limit)
{
    int i, n = 0;

    for (i = 0; i < limit; i++) { if (i == 0) n += 1; if (i == 1) n += 2; if (i == 2) n += 4; if (i == 3) n += 8; if (i == 4) n += 16; if (i == 5) n += 1; }

    return n;
}

static int loop7(int limit)
{
    int i, n = 0;

    for (i = 0; i < limit; i++) { if (i == 0) n += 1; if (i == 1) n += 2; if (i == 2) n += 4; if (i == 3) n += 8; if (i == 4) n += 16; if (i == 5) n += 1; if (i == 6) n += 2; }

    return n;
}

static int loop8(int limit)
{
    int i, n = 0;

    for (i = 0; i < limit; i++) { if (i == 0) n += 1; if (i == 1) n += 2; if (i == 2) n += 4; if (i == 3) n += 8; if (i == 4) n += 16; if (i == 5) n += 1; if (i == 6) n += 2; if (i == 7) n += 4; }

    return n;
}

static int loop9(int limit)
{
    int i, n = 0;

    for (i = 0; i < limit; i++) { if (i == 0) n += 1; if (i == 1) n += 2; if (i == 2) n += 4; if (i == 3) n += 8; if (i == 4) n += 16; if (i == 5) n += 1; if (i == 6) n += 2; if (i == 7) n += 4; if (i == 8) n += 8; }

    return n;
}

static int loop10(int limit)
{
    int i, n = 0;

    for (i = 0; i < limit; i++) { if (i == 0) n += 1; if (i == 1) n += 2; if (i == 2) n += 4; if (i == 3) n += 8; if (i == 4) n += 16; if (i == 5) n += 1; if (i == 6) n += 2; if (i == 7) n += 4; if (i == 8) n += 8; if (i == 9) n += 16; }

    return n;
}

static int loop11(int limit)
{
    int i, n = 0;

    for (i = 0; i < limit; i++) { if (i == 0) n += 1; if (i == 1) n += 2; if (i == 2) n += 4; if (i == 3) n += 8; if (i == 4) n += 16; if (i == 5) n += 1; if (i == 6) n += 2; if (i == 7) n += 4; if (i == 8) n += 8; if (i == 9) n += 16; if (i == 10) n += 1; }

    return n;
}

static int loop12(int limit)
{
    int i, n = 0;

    for (i = 0; i < limit; i++) { if (i == 0) n += 1; if (i == 1) n += 2; if (i == 2) n += 4; if (i == 3) n += 8; if (i == 4) n += 16; if (i == 5) n += 1; if (i == 6) n += 2; if (i == 7) n += 4; if (i == 8) n += 8; if (i == 9) n += 16; if (i == 10) n += 1; if (i == 11) n += 2; }

    return n;
}

int main(void)
{
    if (loop1(8) != 1)
        return loop1(8);
    if (loop2(8) != 3)
        return loop2(8);
    if (loop3(8) != 7)
        return loop3(8);
    if (loop4(8) != 15)
        return loop4(8);
    if (loop5(8) != 31)
        return loop5(8);
    if (loop6(8) != 32)
        return loop6(8);
    if (loop7(8) != 34)
        return loop7(8);
    if (loop8(8) != 38)
        return loop8(8);
    if (loop9(8) != 38)
        return loop9(8);
    if (loop10(8) != 38)
        return loop10(8);
    if (loop11(8) != 38)
        return loop11(8);
    if (loop12(8) != 38)
        return loop12(8);

    return 42;
}
