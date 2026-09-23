/* A frame deeper than (ix+d) reaches.
 *
 * The displacement is one signed byte, so the frame reaches 128 bytes below
 * the frame pointer. A function with both many locals and a statement that
 * spills hard runs out: what a local past the window does is go where the
 * arrays go, but the scratch cannot -- it is where the values being spilled
 * are put, and at that moment there is no register free to work an address
 * out in. So it is reached through IY, which is the backend's own scratch
 * and holds nothing the allocator is using.
 *
 * The function below has forty locals and then a statement that wants sixty
 * bytes of scratch on top of them: a hundred and eighty in all, where a
 * hundred and twenty-eight is what the displacement carries. Its arguments
 * are read from the other side of the frame pointer, which is what the sum
 * of them is for, and the recursion is there because a frame that came back
 * wrong would not come back at all.
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


/* Deeper still: twice the terms, which puts the scratch more than 256 bytes
 * below the frame pointer -- further than one hop reaches, so the hops have
 * to chain. */
static int deeper(int a) {
    int m0=0,m1=1,m2=2,m3=3,m4=4,m5=5,m6=6,m7=7,m8=8,m9=9;
    int m10=10,m11=11,m12=12,m13=13,m14=14,m15=15,m16=16,m17=17,m18=18,m19=19;
    int m20=20,m21=21,m22=22,m23=23,m24=24,m25=25,m26=26,m27=27,m28=28,m29=29;
    int m30=30,m31=31,m32=32,m33=33,m34=34,m35=35,m36=36,m37=37,m38=38,m39=39;
    long long x = 5LL;
    long long heavy;

        heavy = q(x * 1LL) * 2LL + q(x * 2LL) * 3LL + q(x * 3LL) * 4LL
              + q(x * 4LL) * 5LL + q(x * 5LL) * 6LL + q(x * 6LL) * 7LL
              + q(x * 7LL) * 8LL + q(x * 8LL) * 9LL + q(x * 9LL) * 10LL
              + q(x * 10LL) * 11LL;

    return (int) heavy + a + m0 + m39;
}


/* A deep frame with, in the middle of it, every sequence that wants IY for
 * something of its own: a dereference, a narrowing conversion, and a call
 * through a pointer -- the pointer itself being a local too far for the
 * frame pointer to reach, which is its own path through the compiler. */
static int twice(int n) { return n * 2; }
static int thrice(int n) { return n * 3; }

static int mixed(int a, int *p, char *cp) {
    int k0=0,k1=1,k2=2,k3=3,k4=4,k5=5,k6=6,k7=7,k8=8,k9=9;
    int k10=10,k11=11,k12=12,k13=13,k14=14,k15=15,k16=16,k17=17,k18=18,k19=19;
    int k20=20,k21=21,k22=22,k23=23,k24=24,k25=25,k26=26,k27=27,k28=28,k29=29;
    int k30=30,k31=31,k32=32,k33=33,k34=34,k35=35,k36=36,k37=37,k38=38,k39=39;
    long long x = 5LL, heavy;
    int (*fp)(int) = a > 0 ? twice : thrice;
    short s;
    char c;

    heavy = q(x * 1LL) * 2LL + q(x * 2LL) * 3LL + q(x * 3LL) * 4LL
          + q(x * 4LL) * 5LL + q(x * 5LL) * 6LL + q(x * 6LL) * 7LL;

    s = (short) *p;             /* a dereference, and then a narrowing */
    c = *cp;
    heavy += (long long) fp(a); /* and a call through the far pointer */

    return (int) heavy + s + c + k0 + k39;
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
    if (deeper(3) == 2265 + 3 + 39) r++;
    {
        int v = 1000;
        char ch = 7;

        if (mixed(2, &v, &ch) == 587 + 4 + 1000 + 7 + 39) r++;
    }
    if (summed(5, 1, 2, 3, 4, 5) == 15) r++;
    {
        struct three t = made(7, 8, 9);

        if (t.a == 7 && t.b == 8 && t.c == 9) r++;
        if (took(t, 6) == 30) r++;
    }

    return r + 35;              /* 7 checks */
}
