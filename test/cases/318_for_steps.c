/* for loops whose step acc keeps as text and compiles after the body: the
 * condition, the body, the step and one jump back. Nested loops with and
 * without braces, a label straight after a loop, continue and break, steps
 * with commas, calls and pointer walks, a body that declares the step's
 * variable's name again for itself, and the steps that are compiled in
 * their place instead: none, one that names a macro, one with a string. */
static unsigned long h = 1;

static void mix(long v)
{
    h = (h * 31 + (unsigned long) v) & 0xffffffffUL;
}

static int calls;

static int next_of(int i)
{
    calls++;

    return i + 3;
}

#define STEP(x) ((x) += 2)

static int find(int target)
{
    int found = -1, row, col;

    for (row = 0; row < 5; row++)
        for (col = 0; col < 5; col++)
            if (row * 5 + col == target) {
                found = row * 10 + col;
                goto done;
            }
done:
    return found;
}

int main(void)
{
    int i, j, k, n = 0;
    const char *s = "loops";
    const char *p;
    char buf[8];

    for (i = 0; i < 10; i++) {
        if (i == 3)
            continue;
        if (i == 8)
            break;
        n += i;
    }
    mix(n); mix(i);

    for (i = 0, j = 10; i < j; i++, j--)
        mix(i * j);

    for (i = 0; i < 20; i = next_of(i))
        mix(i);
    mix(calls);

    for (p = s; *p; p++)
        mix(*p);

    for (i = 0; i < 4; i++)
        for (j = 0; j < 3; j++)
            mix(i * 3 + j);

    for (i = 0; i < 3; i++) {
        int i = 100;                    /* the body's own, not the step's */

        mix(i);
    }
    mix(i);

    for (i = 0; i < 9; STEP(i))
        mix(i);

    for (i = 0; i < 2; i += (int) sizeof "ab")
        mix(i);

    for (i = 0; i < 5; )
        mix(i++);

    for (k = 0; k < 3; k++)
        for (i = 0; i < 3; i++)
            if (i == k)
                continue;
            else
                mix(k * 10 + i);

    for (i = 0; i < 7; i++)
        buf[i] = (char) ('a' + i);
    buf[7] = 0;
    for (i = 0; buf[i]; i++)
        ;
    mix(i);

    mix(find(13)); mix(find(99));

    /* A macro in the step, defined again in the body: the step is the one
     * written, expanded where it is, so it adds 1 every time. */
#define INC 1
    for (i = 0; i < 6; i += INC) {
#undef INC
#define INC 2
        mix(i);
    }
#undef INC

    /* A string in the step, and a string the first token after the loop. */
    for (i = 0; i < 9; i += (int) sizeof "abcd")
        mix(i);
    "xyz"[0] == 'x' ? mix(1) : mix(2);

    /* From gcc on the host. */
    return h == 1242038904UL ? 42 : 1;
}
