/* Two-byte values read while another value is held in DE.
 *
 * Reading a short, through a pointer or from the frame, once kept its low
 * byte in E while the high one was fetched -- and when the value before it
 * had been moved out of HL into DE to make room, that cost the earlier value
 * its low byte: `*c + *s` with c pointing at 10 and s at 60 came to 120. The
 * same went for narrowing a value to a short, and for pushing a long
 * argument. Each case below has something live in DE at the moment it
 * happens.
 */
int add_through(char *c, short *s) {
    return *c + *s;
}

int mix_through(short *a, unsigned short *b, char *c) {
    return (*c + *a) - (*b - *c);
}

int frame_shorts(int x, short s, unsigned short u) {
    int y = x + 1;

    return (y ^ s) + (y | u);
}

long pass_long(long a, int b, long c) {
    return a + b + c;
}

int main(void) {
    int r = 0;
    char c = 10;
    short s = 60;
    short neg = -300;
    unsigned short big = 60000;
    int i = 7;
    short narrowed;

    if (add_through(&c, &s) == 70) r = r + 1;
    if (mix_through(&neg, &big, &c) == -60280) r = r + 1;
    if (frame_shorts(5, -2, 40000) == 39998) r = r + 1;

    /* Narrowed to a short on the way into it, with i in a register. */
    narrowed = i * 10000;
    if (narrowed + i == 4471) r = r + 1;

    if (pass_long(100000, i, 200000) == 300007) r = r + 1;

    /* 5 */
    return r + 37;
}
