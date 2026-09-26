/* Stores to globals and statics, which acc writes to the address itself --
 * ld (nn), hl, de or bc, and ld (nn), a -- where the address is a constant
 * of this file: variables with an initial value, which are in the image,
 * and without, which are in the bss; ints, pointers, chars, and the shorts
 * and longs beside them; members and elements at constant offsets; values
 * that are constants, locals, results in each register and bytes, and the
 * value an assignment comes to, used again. */
static unsigned long h = 1;

static void mix(long v)
{
    h = (h * 31 + (unsigned long) v) & 0xffffffffUL;
}

struct rec { char c; int n; unsigned char u; short s; long l; int *p; };

int di = 1;                     /* with a value: in the image */
char dc = 2;
int bi;                         /* without: in the bss */
char bc;
unsigned char bu;
signed char bs;
short bsh;
long bl;
int *bp;
int barr[4];
char bcarr[3];
struct rec brec;
static int sdi = 5;
static int sbi;

static int twice(int x) { return x + x; }

int main(void)
{
    int k = 70000, i, t;
    char c = -9;
    unsigned char u = 201;

    di = k; dc = c; bi = -k; bc = 'q'; bu = u; bs = c; bsh = -1234; bl = 123456789L;
    mix(di); mix(dc); mix(bi); mix(bc); mix(bu); mix(bs); mix(bsh); mix(bl);
    bp = &barr[2];
    barr[0] = 11; barr[3] = k + 1; bcarr[1] = u; bcarr[2] = (char) (c + u);
    brec.c = 'r'; brec.n = twice(k); brec.u = 255; brec.s = 99; brec.l = -5L; brec.p = &bi;
    mix(*bp); mix(barr[0]); mix(barr[3]); mix(bcarr[1]); mix(bcarr[2]);
    mix(brec.c); mix(brec.n); mix(brec.u); mix(brec.s); mix(brec.l); mix(*brec.p);
    sdi = sbi = t = k - 1;
    mix(sdi); mix(sbi); mix(t);
    mix(bi = di + 1);
    mix(bc = (char) (c * 3));
    mix(bu = (unsigned char) (u + 100));
    for (i = 0; i < 4; i++) {
        bi += i;
        bc = (char) (bc + i);
        mix(bi); mix(bc);
    }
    i = (di = 3) + (bi = 4);
    mix(i);

    /* With HL, and then DE, holding something, the value is in DE or BC. */
    mix(twice(k) + (bi = k - 7));
    mix(bi);
    mix(twice(k) + (twice(3) + (sbi = k + 9)));
    mix(sbi);
    mix(twice(k) + (bi = k));
    mix(bi);
    mix(twice(k) + (twice(5) * (sbi = t)));
    mix(sbi);
    {
        int *q = &barr[1], r = 17;

        mix(*q + (di = r) + (bi = k));
        mix(di + bi);
        mix((di = r) + ((bi = k) + (sbi = t)));
        mix(di + bi + sbi);
    }

    /* From gcc on the host. */
    return h == 618213274UL ? 42 : 1;
}
