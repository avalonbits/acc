/* Two values that fit in a byte, compared in A: unsigned chars as locals,
 * globals and reads through pointers, masks of them, ORs with a small
 * constant, and constants -- every operator, each way round, at 0, 1, 127,
 * 128, 254 and 255. And with another value live in HL, where the answer
 * is made: `x + (a == b)` kept x. */

typedef unsigned char u8;

u8 values[6] = { 0, 1, 127, 128, 254, 255 };
u8 ga, gb;

/* Each comparison a bit of the answer, the six operators in turn. */
#define ALL(a, b)                                                         \
    (((a) == (b)) + 2 * ((a) != (b)) + 4 * ((a) < (b)) + 8 * ((a) <= (b)) \
     + 16 * ((a) > (b)) + 32 * ((a) >= (b)))

__attribute__((noinline)) int locals(u8 a, u8 b)
{
    return ALL(a, b);
}

__attribute__((noinline)) int globals(void)
{
    return ALL(ga, gb);
}

__attribute__((noinline)) int pointers(const u8 *p, const u8 *q)
{
    return ALL(*p, *q);
}

__attribute__((noinline)) int masks(u8 a, const u8 *q)
{
    return ALL(a & 0x7f, *q & 0xf0);
}

__attribute__((noinline)) int ors(u8 a, u8 b)
{
    return ALL((a & 0x0f) | 0x20, b | 1);
}

/* Two bytes wide, by the OR: not compared by the low one alone. */
__attribute__((noinline)) int wider(u8 a, u8 b)
{
    return ALL(a | 0x100, b);
}

/* Bitfields, which are not their bytes. */
struct fields { unsigned char low : 3, high : 5; };

__attribute__((noinline)) int fields(int a, int b)
{
    struct fields f;

    f.low = (unsigned char) a;
    f.high = (unsigned char) b;

    return ALL(f.low, f.high);
}

/* Signed, which is not a byte's order. */
signed char sa, sb;

__attribute__((noinline)) int signs(signed char a, signed char b)
{
    sa = a;
    sb = b;

    return ALL(sa, sb);
}

__attribute__((noinline)) int signed_less(signed char a, signed char b)
{
    signed char x = a, y = b;

    return x < y;
}

/* A byte that byte arithmetic left in A, against a constant: unsigned
 * and signed, at the ends of each range. */
__attribute__((noinline)) int in_a(u8 w, signed char v)
{
    return (--w > 0) + 2 * (--w < 128) + 4 * (++v > 0) + 8 * (--v < -1)
           + 16 * (++v >= 127) + 32 * (++w == 255);
}

/* And against another byte: a local, read where it is, and a global,
 * read through A, where the first is. */
__attribute__((noinline)) int in_a_bytes(u8 w, u8 x)
{
    ga = x;

    return (--w == x) + 2 * (--w < ga) + 4 * (++w >= ga);
}

__attribute__((noinline)) int constants(u8 a)
{
    return ALL(a, 128) + 64 * ALL(a & 0xfe, 255) + 4096 * ALL(0, a);
}

/* Equal by the low byte and not by the whole: an int is no byte. */
__attribute__((noinline)) int wide(u8 a, int b)
{
    return ALL(a, b);
}

__attribute__((noinline)) int kept(int x, u8 a, u8 b)
{
    return x * 2 + (a == b) + ((a & 7) > (b & 3)) * 10 + (a < b) * 100;
}

int reference(int a, int b)
{
    return ALL(a, b);
}

int main(void)
{
    int i, j;

    for (i = 0; i != 6; i++)
        for (j = 0; j != 6; j++) {
            int a = values[i], b = values[j];

            ga = (u8) a;
            gb = (u8) b;
            if (locals((u8) a, (u8) b) != reference(a, b))
                return 1;
            if (globals() != reference(a, b))
                return 2;
            if (pointers(&values[i], &values[j]) != reference(a, b))
                return 3;
            if (masks((u8) a, &values[j]) != reference(a & 0x7f, b & 0xf0))
                return 4;
            if (ors((u8) a, (u8) b) != reference((a & 0x0f) | 0x20, b | 1))
                return 5;
            if (in_a_bytes((u8) a, (u8) b)
                != ((u8) (a - 1) == b) + 2 * ((u8) (a - 2) < b) + 4 * ((u8) (a - 1) >= b))
                return 14;
            if (wide((u8) a, b + 256) != reference(a, b + 256))
                return 6;
            if (wider((u8) a, (u8) b) != reference(a | 0x100, b))
                return 9;
            if (fields(a, b) != reference(a & 7, b & 31))
                return 12;
            if (signs((signed char) a, (signed char) b)
                != reference((signed char) a, (signed char) b))
                return 10;
            if (signed_less((signed char) a, (signed char) b)
                != ((signed char) a < (signed char) b))
                return 11;
            if (kept(1000, (u8) a, (u8) b)
                != 2000 + (a == b) + ((a & 7) > (b & 3)) * 10 + (a < b) * 100)
                return 7;
        }
    for (i = 0; i != 6; i++) {
        int a = values[i];
        u8 w = (u8) a;
        signed char v = (signed char) a;
        int want, one, two, three, four, five, six;

        w--;
        one = w > 0;
        w--;
        two = w < 128;
        v++;
        three = v > 0;
        v--;
        four = v < -1;
        v++;
        five = v >= 127;
        w++;
        six = w == 255;
        want = one + 2 * two + 4 * three + 8 * four + 16 * five + 32 * six;
        if (in_a((u8) a, (signed char) a) != want)
            return 13;

        if (constants((u8) a)
            != reference(a, 128) + 64 * reference(a & 0xfe, 255) + 4096 * reference(0, a))
            return 8;
    }

    return 42;
}
