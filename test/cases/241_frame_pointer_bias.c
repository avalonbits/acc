/* A frame larger than a signed byte reaches, without anything going far.
 *
 * (ix+d) reaches 128 bytes either side of the frame pointer. The frame
 * pointer used to sit at the saved ix, which put the arguments in the
 * positive half and the whole of the frame in the negative one -- and a
 * function's arguments are a few bytes where its frame is everything else,
 * so nearly half of what the instruction can address went unused. It is now
 * put as far below the arguments as it can go and still reach the last of
 * them, and the frame has both halves.
 *
 * The function below has forty locals and then a statement that wants sixty
 * bytes of scratch on top of them: a hundred and eighty in all, which does
 * not fit in a hundred and twenty-eight however the halves are divided. The
 * arguments still have to be read correctly from the other side of the
 * frame pointer, which is what the sum of them is for, and the epilogue has
 * to put the stack back where it found it, which is what the recursion is
 * for -- a return that left sp wrong would not come back at all.
 */
static long long q(long long v) { return v + 1LL; }

static int wide_frame(int a, int b, int c, long d) {
    int n0=0,n1=1,n2=2,n3=3,n4=4,n5=5,n6=6,n7=7,n8=8,n9=9;
    int n10=10,n11=11,n12=12,n13=13,n14=14,n15=15,n16=16,n17=17,n18=18,n19=19;
    int n20=20,n21=21,n22=22,n23=23,n24=24,n25=25,n26=26,n27=27,n28=28,n29=29;
    int n30=30,n31=31,n32=32,n33=33,n34=34,n35=35,n36=36,n37=37,n38=38,n39=39;
    long long x = 5LL;
    long long heavy;

    /* Six eight-byte terms in one statement, which is the scratch. */
    heavy = q(x * 1LL) * 2LL + q(x * 2LL) * 3LL + q(x * 3LL) * 4LL
          + q(x * 4LL) * 5LL + q(x * 5LL) * 6LL + q(x * 6LL) * 7LL;

    n39 = n0+n1+n2+n3+n4+n5+n6+n7+n8+n9
        + n10+n11+n12+n13+n14+n15+n16+n17+n18+n19
        + n20+n21+n22+n23+n24+n25+n26+n27+n28+n29
        + n30+n31+n32+n33+n34+n35+n36+n37+n38;

    /* 741 from the locals, 587 from the heavy statement, and the arguments
     * from above the frame pointer. */
    return n39 + (int) heavy + a + b + c + (int) d;
}

/* Deep enough that an epilogue leaving sp wrong would run the stack away. */
static int down(int n) {
    int pad[8];
    int i;

    for (i = 0; i < 8; i++)
        pad[i] = n + i;
    if (n == 0)
        return pad[0];

    return down(n - 1) + pad[0] - n;
}

/* Arguments read through a frame pointer that is below them, including one
 * a variadic function has to find the address of. */
static int summed(int n, ...) {
    va_list ap;
    int s = 0;

    va_start(ap, n);
    while (n--)
        s += va_arg(ap, int);
    va_end(ap);

    return s;
}

struct three { int a, b, c; };

static struct three made(int a, int b, int c) {
    struct three t;

    t.a = a; t.b = b; t.c = c;

    return t;
}

static int took(struct three t, int extra) {
    return t.a + t.b + t.c + extra;
}

int main(void) {
    int r = 0;

    if (wide_frame(100, 200, 300, 400L) == 741 + 587 + 1000) r++;
    if (down(40) == 0) r++;
    if (summed(5, 1, 2, 3, 4, 5) == 15) r++;
    {
        struct three t = made(7, 8, 9);

        if (t.a == 7 && t.b == 8 && t.c == 9) r++;
        if (took(t, 6) == 30) r++;
    }

    return r + 37;              /* 5 checks */
}
