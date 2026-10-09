/* A signed char compared with an unsigned constant: an unsigned
 * comparison, as C's conversions make it, a negative char a large
 * unsigned -- `c < 10u` false for c of -3. acc compared the byte signed
 * by its own type, the leaf backend too: both made it true. Each order and
 * each equality, the constant on either side, against constants up to
 * 255u; the char a local in its slot and one read through a pointer. */

signed char seeds[5] = { -128, -3, 0, 10, 127 };

__attribute__((noinline)) static long in_slot(int k)
{
    signed char c = seeds[k];
    long bits = 0, bit = 1;

    bits += (c < 0u) * bit;
    bit = bit * 3 % 1000003;
    bits += (0u < c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c <= 0u) * bit;
    bit = bit * 3 % 1000003;
    bits += (0u <= c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c > 0u) * bit;
    bit = bit * 3 % 1000003;
    bits += (0u > c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c >= 0u) * bit;
    bit = bit * 3 % 1000003;
    bits += (0u >= c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c == 0u) * bit;
    bit = bit * 3 % 1000003;
    bits += (0u == c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c != 0u) * bit;
    bit = bit * 3 % 1000003;
    bits += (0u != c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c < 1u) * bit;
    bit = bit * 3 % 1000003;
    bits += (1u < c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c <= 1u) * bit;
    bit = bit * 3 % 1000003;
    bits += (1u <= c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c > 1u) * bit;
    bit = bit * 3 % 1000003;
    bits += (1u > c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c >= 1u) * bit;
    bit = bit * 3 % 1000003;
    bits += (1u >= c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c == 1u) * bit;
    bit = bit * 3 % 1000003;
    bits += (1u == c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c != 1u) * bit;
    bit = bit * 3 % 1000003;
    bits += (1u != c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c < 10u) * bit;
    bit = bit * 3 % 1000003;
    bits += (10u < c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c <= 10u) * bit;
    bit = bit * 3 % 1000003;
    bits += (10u <= c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c > 10u) * bit;
    bit = bit * 3 % 1000003;
    bits += (10u > c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c >= 10u) * bit;
    bit = bit * 3 % 1000003;
    bits += (10u >= c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c == 10u) * bit;
    bit = bit * 3 % 1000003;
    bits += (10u == c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c != 10u) * bit;
    bit = bit * 3 % 1000003;
    bits += (10u != c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c < 127u) * bit;
    bit = bit * 3 % 1000003;
    bits += (127u < c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c <= 127u) * bit;
    bit = bit * 3 % 1000003;
    bits += (127u <= c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c > 127u) * bit;
    bit = bit * 3 % 1000003;
    bits += (127u > c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c >= 127u) * bit;
    bit = bit * 3 % 1000003;
    bits += (127u >= c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c == 127u) * bit;
    bit = bit * 3 % 1000003;
    bits += (127u == c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c != 127u) * bit;
    bit = bit * 3 % 1000003;
    bits += (127u != c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c < 128u) * bit;
    bit = bit * 3 % 1000003;
    bits += (128u < c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c <= 128u) * bit;
    bit = bit * 3 % 1000003;
    bits += (128u <= c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c > 128u) * bit;
    bit = bit * 3 % 1000003;
    bits += (128u > c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c >= 128u) * bit;
    bit = bit * 3 % 1000003;
    bits += (128u >= c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c == 128u) * bit;
    bit = bit * 3 % 1000003;
    bits += (128u == c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c != 128u) * bit;
    bit = bit * 3 % 1000003;
    bits += (128u != c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c < 200u) * bit;
    bit = bit * 3 % 1000003;
    bits += (200u < c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c <= 200u) * bit;
    bit = bit * 3 % 1000003;
    bits += (200u <= c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c > 200u) * bit;
    bit = bit * 3 % 1000003;
    bits += (200u > c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c >= 200u) * bit;
    bit = bit * 3 % 1000003;
    bits += (200u >= c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c == 200u) * bit;
    bit = bit * 3 % 1000003;
    bits += (200u == c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c != 200u) * bit;
    bit = bit * 3 % 1000003;
    bits += (200u != c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c < 255u) * bit;
    bit = bit * 3 % 1000003;
    bits += (255u < c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c <= 255u) * bit;
    bit = bit * 3 % 1000003;
    bits += (255u <= c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c > 255u) * bit;
    bit = bit * 3 % 1000003;
    bits += (255u > c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c >= 255u) * bit;
    bit = bit * 3 % 1000003;
    bits += (255u >= c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c == 255u) * bit;
    bit = bit * 3 % 1000003;
    bits += (255u == c) * bit;
    bit = bit * 3 % 1000003;
    bits += (c != 255u) * bit;
    bit = bit * 3 % 1000003;
    bits += (255u != c) * bit;
    bit = bit * 3 % 1000003;
    return bits;
}

__attribute__((noinline)) static long through(const signed char *p)
{
    long bits = 0;

    bits += (*p < 10u) + 2 * (*p >= 128u) + 4 * (*p > 0u) + 8 * (*p <= 200u)
            + 16 * (*p == 255u) + 32 * (*p != 253u) + 64 * (5u < *p);
    return bits;
}

#define CHECK(expr, want) { check++; if ((long) (expr) != (long) (want)) return check; }

int main(void)
{
    int check = 0;

    CHECK(in_slot(0), 19995724L)
    CHECK(through(&seeds[0]), 102L)
    CHECK(in_slot(1), 19995724L)
    CHECK(through(&seeds[1]), 102L)
    CHECK(in_slot(2), 18890405L)
    CHECK(through(&seeds[2]), 41L)
    CHECK(in_slot(3), 18808550L)
    CHECK(through(&seeds[3]), 108L)
    CHECK(in_slot(4), 20456571L)
    CHECK(through(&seeds[4]), 108L)

    return 42;
}
