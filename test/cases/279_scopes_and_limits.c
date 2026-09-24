/* More that gcc.dg found acc refusing, all C99:
 * - a statement's body is a block of its own, and so is the statement, so
 *   a tag it defines ends with it and may be defined again inside
 *   (6.8.4p3, 6.8.5p5): c99-scope-2 does this in every kind of statement;
 * - a declaration that begins with `restrict`;
 * - a parameter of a VLA typedef's type, which is a pointer;
 * - a macro with 40 parameters: C99 5.2.4.1 asks for 127, and acc took 16. */
#define SUM(p0,p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11,p12,p13,p14,p15,p16,p17,p18,p19,p20,p21,p22,p23,p24,p25,p26,p27,p28,p29,p30,p31,p32,p33,p34,p35,p36,p37,p38,p39) (p0+p1+p2+p3+p4+p5+p6+p7+p8+p9+p10+p11+p12+p13+p14+p15+p16+p17+p18+p19+p20+p21+p22+p23+p24+p25+p26+p27+p28+p29+p30+p31+p32+p33+p34+p35+p36+p37+p38+p39)

typedef int *iptr;

static int second(int *p)
{
    return p[1];
}

int main(void)
{
    struct foo { int i0; };
    int r = 0, a = sizeof (struct foo), b, c = 0, k;

    if (b = sizeof (struct foo { int i0; int i1; }))
        c = sizeof (struct foo { int i0; int i1; int i2; });
    if (a < b && b < c) r++;
    for (k = 0; k < 1; k++)
        c = sizeof (struct foo { char x[9]; });
    while (sizeof (struct foo { char y[5]; }) != 5)
        ;
    if (c == 9 && sizeof (struct foo) == a) r++;
    {
        int x = 20, n = 3;
        restrict iptr q = &x;
        typedef int row[n];
        int take(row);
        int v[3] = { 0, 21, 0 };

        if (*q + second(v) == 41) r++;
    }
    if (SUM(0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1) == 20) r++;

    return r + 38;              /* 4 checks */
}
