/* static: file-scope names, and locals that keep their value between calls
 * -- initialised once, from a constant, and zero when they are not. */
static int hidden = 5;
static int helper(int x);

int counter(void) {
    static int calls;
    static int start = 100;

    calls++;
    start += 10;

    return calls * 1000 + start;
}

int fill(int i) {
    static char buf[8] = "abc";
    static struct { int n; char c; } state = { 1, 'x' };

    buf[i] = 'z';
    state.n += i;

    return buf[0] + buf[i] + state.n + state.c;
}

/* Two statics of one name, one in each function: each is its own. */
int tick(void) {
    static int n;

    return ++n;
}

int tock(void) {
    static int n = 100;

    return ++n;
}

int main(void) {
    int r = 0;

    if (counter() == 1110 && counter() == 2120) r++;
    if (hidden == 5 && helper(2) == 7) r++;
    if (fill(1) == 'a' + 'z' + 2 + 'x') r++;
    if (fill(2) == 'a' + 'z' + 4 + 'x') r++;
    {
        static int inner = 3;

        inner++;
        if (inner == 4) r++;
    }

    tick();
    tock();
    if (tick() == 2 && tock() == 102) r++;

    return r + 36;          /* 6 checks */
}

static int helper(int x) {
    return x + hidden;
}
