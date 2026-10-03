/* Chars compared, and masked, a byte at a time in A -- as opt-acc's
 * machine-level backend does where both sides are bytes widened the same
 * way: signed against signed, moved by 0x80 so that the order is an
 * unsigned one; unsigned against unsigned; a constant on either side, the
 * one on the left turned to the right; constants with no next one; and
 * &, | and ^ whose answers are bytes widened as their operands say. */
int below(signed char a, signed char b) { return a < b; }
int at_most(signed char a, signed char b) { return a <= b; }
int ubelow(unsigned char a, unsigned char b) { return a < b; }
int under10(signed char c) { return c < 10; }
int over_minus(signed char c) { return -3 < c; }
int not_above(signed char c) { return 100 >= c; }
int digit(const char *p) { return *p >= '0' && *p <= '9'; }
int top_byte(unsigned char c) { return c > 254; }
int maxed(int x) { return 0x7fffff > x; }
int never(int x) { return 0x7fffff < x; }
int unever(unsigned x) { return 0xffffffu < x; }
int lower(signed char c) { return (c | 0x20) == 'a'; }
int low_bits(signed char c) { return c & 0x0f; }
int high_bits(signed char c) { return c & 0xf0; }
int flipped(signed char c) { return c ^ 1; }

int main(void)
{
    int r = 0;
    char s[3] = { '7', 'x', 0 };

    r += below(-5, 3) && !below(3, -5) && !below(-1, -1);
    r += at_most(-128, 127) && at_most(-1, -1) && !at_most(5, -5);
    r += ubelow(3, 250) && !ubelow(250, 3);
    r += under10(-1) && under10(9) && !under10(10);
    r += over_minus(-2) && !over_minus(-3) && !over_minus(-100);
    r += not_above(100) && not_above(-128) && !not_above(101);
    r += digit(s) && !digit(s + 1);
    r += top_byte(255) && !top_byte(254);
    r += maxed(0x7ffffe) && !maxed(0x7fffff) && maxed(-1);
    r += !never(0x7fffff) && !never(-1) && !unever(0xffffffu) && !unever(5u);
    r += lower('A') && lower('a') && !lower('B');
    r += low_bits(-1) == 15 && low_bits(0x13) == 3;
    r += high_bits(-1) == 240 && high_bits(0x13) == 16;
    r += flipped(-1) == -2 && flipped(4) == 5;

    return r == 14 ? 42 : r;
}
