/* What opt-acc's machine-level backend makes of constants and of what it
 * knows of a value's bits: signed comparisons with a constant -- against
 * 0 by the sign alone, the rest through the bias with the constant moved
 * already -- at the ends of the range; &, | and ^ with a constant, made a
 * byte at a time or not at all; elements of 3, 5, 6, 7 and 12 bytes found
 * by adds; and values known to be bytes, or not negative -- a mask, a
 * remainder, a shift, a _Bool read -- compared and tested as such. */
#include <limits.h>
#include <stdbool.h>

int below0(int x) { return x < 0; }
int atleast0(int x) { return x >= 0; }
int above0(int x) { return x > 0; }
int below5(int x) { return x < 5; }
int atmost_m3(int x) { return x <= -3; }
int above_max(int x) { return x > INT_MAX - 1; }

unsigned keep_all(unsigned x) { return x & 0xffffff; }
unsigned low7(unsigned x) { return x & 0x7f; }
unsigned set_bit7(unsigned x) { return x | 0x80; }
unsigned flip_two(unsigned x) { return x ^ 0x0101; }
unsigned clear_mid(unsigned x) { return x & 0xff00ff; }
int keep_sign(signed char c) { return c & 0xffffff; }

struct e5 { char c[5]; };
struct e6 { int a, b; };
struct e7 { char c[7]; };
struct e12 { int a[4]; };
int at3(int *p, int i) { return p[i]; }
int at5(struct e5 *p, int i) { return p[i].c[4]; }
int at6(struct e6 *p, int i) { return p[i].b; }
int at7(struct e7 *p, int i) { return p[i].c[6]; }
int at12(struct e12 *p, int i) { return p[i].a[3]; }

bool flag;
int masked_small(int x) { return (x & 0x7f) < 100; }
int rem_small(unsigned x) { return x % 10 == 7; }
int shifted(unsigned x) { return (x >> 16) > 200; }
int nonneg_cmp(int x, int y) { return (x & 0x7fff) < (y & 0x7fff); }
int flag_set(void) { if (flag) return 3; return 4; }

int main(void)
{
    int a[5] = { 1, 2, 3, 4, 5 };
    struct e5 b[3];
    struct e6 c[3] = { { 1, 2 }, { 3, 4 }, { 5, 6 } };
    struct e7 d[3];
    struct e12 e[3];
    int r = 0;

    r += below0(-1) && !below0(0) && below0(INT_MIN) && !below0(INT_MAX);
    r += atleast0(0) && !atleast0(-1) && atleast0(INT_MAX) && !atleast0(INT_MIN);
    r += above0(1) && !above0(0) && !above0(INT_MIN) && above0(INT_MAX);
    r += below5(4) && !below5(5) && below5(INT_MIN) && !below5(INT_MAX);
    r += atmost_m3(-3) && !atmost_m3(-2) && atmost_m3(INT_MIN) && !atmost_m3(INT_MAX);
    r += above_max(INT_MAX) && !above_max(INT_MAX - 1) && !above_max(INT_MIN);

    r += keep_all(0x123456) == 0x123456 && low7(0x1234ff) == 0x7f;
    r += set_bit7(0x123401) == 0x123481 && flip_two(0x123456) == 0x123557;
    r += clear_mid(0x123456) == 0x120056 && keep_sign(-1) == -1;

    b[2].c[4] = 9;
    d[2].c[6] = 11;
    e[2].a[3] = 13;
    r += at3(a, 3) == 4 && at5(b, 2) == 9 && at6(c, 2) == 6;
    r += at7(d, 2) == 11 && at12(e, 2) == 13;

    r += masked_small(99) && !masked_small(0x7f) && masked_small(0x1263);
    r += rem_small(17) && !rem_small(18) && !rem_small(0xfffffb);
    r += shifted(0xc90000) && !shifted(0xc80000) && !shifted(0);
    r += nonneg_cmp(0x8001, 2) && !nonneg_cmp(0x7fff, 0x7ffe);
    r += flag_set() == 4;
    flag = true;
    r += flag_set() == 3;

    return r == 17 ? 42 : r;
}
