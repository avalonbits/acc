/* A wide operator whose left operand is a variable: the routine's entry
 * that copies it into the answer's slot first, from the address in BC.
 * Every operator, at each width, into a variable and into scratch, with
 * the operands read again after to show the copy left them as they were. */

static int add_l(long a, long b)
{
    long r = a + b;

    return r == (-123382224L) && (a + b) == (-123382224L) && a == (-123456789L) && b == (74565L);
}

static int sub_l(long a, long b)
{
    long r = a - b;

    return r == (-123531354L) && (a - b) == (-123531354L) && a == (-123456789L) && b == (74565L);
}

static int and_l(long a, long b)
{
    long r = a & b;

    return r == (8769L) && (a & b) == (8769L) && a == (-123456789L) && b == (74565L);
}

static int or_l(long a, long b)
{
    long r = a | b;

    return r == (-123390993L) && (a | b) == (-123390993L) && a == (-123456789L) && b == (74565L);
}

static int xor_l(long a, long b)
{
    long r = a ^ b;

    return r == (-123399762L) && (a ^ b) == (-123399762L) && a == (-123456789L) && b == (74565L);
}

static int mul_l(long a, long b)
{
    long r = a * b;

    return r == (-1440556457L) && (a * b) == (-1440556457L) && a == (-123456789L) && b == (74565L);
}

static int div_l(long a, long b)
{
    long r = a / b;

    return r == (-1655L) && (a / b) == (-1655L) && a == (-123456789L) && b == (74565L);
}

static int rem_l(long a, long b)
{
    long r = a % b;

    return r == (-51714L) && (a % b) == (-51714L) && a == (-123456789L) && b == (74565L);
}

static int shl_l(long a, long b)
{
    long r = a << b;

    return r == (344350048L) && (a << b) == (344350048L) && a == (-123456789L) && b == (5L);
}

static int shr_l(long a, long b)
{
    long r = a >> b;

    return r == (-3858025L) && (a >> b) == (-3858025L) && a == (-123456789L) && b == (5L);
}

static int add_ul(unsigned long a, unsigned long b)
{
    unsigned long r = a + b;

    return r == 0xf8a55630UL && (a + b) == 0xf8a55630UL && a == 0xf8a432ebUL && b == 0x12345UL;
}

static int sub_ul(unsigned long a, unsigned long b)
{
    unsigned long r = a - b;

    return r == 0xf8a30fa6UL && (a - b) == 0xf8a30fa6UL && a == 0xf8a432ebUL && b == 0x12345UL;
}

static int and_ul(unsigned long a, unsigned long b)
{
    unsigned long r = a & b;

    return r == 0x2241UL && (a & b) == 0x2241UL && a == 0xf8a432ebUL && b == 0x12345UL;
}

static int or_ul(unsigned long a, unsigned long b)
{
    unsigned long r = a | b;

    return r == 0xf8a533efUL && (a | b) == 0xf8a533efUL && a == 0xf8a432ebUL && b == 0x12345UL;
}

static int xor_ul(unsigned long a, unsigned long b)
{
    unsigned long r = a ^ b;

    return r == 0xf8a511aeUL && (a ^ b) == 0xf8a511aeUL && a == 0xf8a432ebUL && b == 0x12345UL;
}

static int mul_ul(unsigned long a, unsigned long b)
{
    unsigned long r = a * b;

    return r == 0xaa22da57UL && (a * b) == 0xaa22da57UL && a == 0xf8a432ebUL && b == 0x12345UL;
}

static int div_ul(unsigned long a, unsigned long b)
{
    unsigned long r = a / b;

    return r == 0xda88UL && (a / b) == 0xda88UL && a == 0xf8a432ebUL && b == 0x12345UL;
}

static int rem_ul(unsigned long a, unsigned long b)
{
    unsigned long r = a % b;

    return r == 0xb443UL && (a % b) == 0xb443UL && a == 0xf8a432ebUL && b == 0x12345UL;
}

static int shl_ul(unsigned long a, unsigned long b)
{
    unsigned long r = a << b;

    return r == 0x14865d60UL && (a << b) == 0x14865d60UL && a == 0xf8a432ebUL && b == 0x5UL;
}

static int shr_ul(unsigned long a, unsigned long b)
{
    unsigned long r = a >> b;

    return r == 0x7c52197UL && (a >> b) == 0x7c52197UL && a == 0xf8a432ebUL && b == 0x5UL;
}

static int add_ll(long long a, long long b)
{
    long long r = a + b;

    return r == (-1229681171778LL) && (a + b) == (-1229681171778LL) && a == (-1234567890123LL) && b == (4886718345LL);
}

static int sub_ll(long long a, long long b)
{
    long long r = a - b;

    return r == (-1239454608468LL) && (a - b) == (-1239454608468LL) && a == (-1234567890123LL) && b == (4886718345LL);
}

static int and_ll(long long a, long long b)
{
    long long r = a & b;

    return r == (33841921LL) && (a & b) == (33841921LL) && a == (-1234567890123LL) && b == (4886718345LL);
}

static int or_ll(long long a, long long b)
{
    long long r = a | b;

    return r == (-1229715013699LL) && (a | b) == (-1229715013699LL) && a == (-1234567890123LL) && b == (4886718345LL);
}

static int xor_ll(long long a, long long b)
{
    long long r = a ^ b;

    return r == (-1229748855620LL) && (a ^ b) == (-1229748855620LL) && a == (-1234567890123LL) && b == (4886718345LL);
}

static int mul_ll(long long a, long long b)
{
    long long r = a * b;

    return r == (-900244708985028003LL) && (a * b) == (-900244708985028003LL) && a == (-1234567890123LL) && b == (4886718345LL);
}

static int div_ll(long long a, long long b)
{
    long long r = a / b;

    return r == (-252LL) && (a / b) == (-252LL) && a == (-1234567890123LL) && b == (4886718345LL);
}

static int rem_ll(long long a, long long b)
{
    long long r = a % b;

    return r == (-3114867183LL) && (a % b) == (-3114867183LL) && a == (-1234567890123LL) && b == (4886718345LL);
}

static int shl_ll(long long a, long long b)
{
    long long r = a << b;

    return r == (-39506172483936LL) && (a << b) == (-39506172483936LL) && a == (-1234567890123LL) && b == (5LL);
}

static int shr_ll(long long a, long long b)
{
    long long r = a >> b;

    return r == (-38580246567LL) && (a >> b) == (-38580246567LL) && a == (-1234567890123LL) && b == (5LL);
}

static int add_ull(unsigned long long a, unsigned long long b)
{
    unsigned long long r = a + b;

    return r == 0xfffffee1b14a62beULL && (a + b) == 0xfffffee1b14a62beULL && a == 0xfffffee08e04fb35ULL && b == 0x123456789ULL;
}

static int sub_ull(unsigned long long a, unsigned long long b)
{
    unsigned long long r = a - b;

    return r == 0xfffffedf6abf93acULL && (a - b) == 0xfffffedf6abf93acULL && a == 0xfffffee08e04fb35ULL && b == 0x123456789ULL;
}

static int and_ull(unsigned long long a, unsigned long long b)
{
    unsigned long long r = a & b;

    return r == 0x2046301ULL && (a & b) == 0x2046301ULL && a == 0xfffffee08e04fb35ULL && b == 0x123456789ULL;
}

static int or_ull(unsigned long long a, unsigned long long b)
{
    unsigned long long r = a | b;

    return r == 0xfffffee1af45ffbdULL && (a | b) == 0xfffffee1af45ffbdULL && a == 0xfffffee08e04fb35ULL && b == 0x123456789ULL;
}

static int xor_ull(unsigned long long a, unsigned long long b)
{
    unsigned long long r = a ^ b;

    return r == 0xfffffee1ad419cbcULL && (a ^ b) == 0xfffffee1ad419cbcULL && a == 0xfffffee08e04fb35ULL && b == 0x123456789ULL;
}

static int mul_ull(unsigned long long a, unsigned long long b)
{
    unsigned long long r = a * b;

    return r == 0xf381b034f605c25dULL && (a * b) == 0xf381b034f605c25dULL && a == 0xfffffee08e04fb35ULL && b == 0x123456789ULL;
}

static int div_ull(unsigned long long a, unsigned long long b)
{
    unsigned long long r = a / b;

    return r == 0xe0ffff03ULL && (a / b) == 0xe0ffff03ULL && a == 0xfffffee08e04fb35ULL && b == 0x123456789ULL;
}

static int rem_ull(unsigned long long a, unsigned long long b)
{
    unsigned long long r = a % b;

    return r == 0x1009c4d9aULL && (a % b) == 0x1009c4d9aULL && a == 0xfffffee08e04fb35ULL && b == 0x123456789ULL;
}

static int shl_ull(unsigned long long a, unsigned long long b)
{
    unsigned long long r = a << b;

    return r == 0xffffdc11c09f66a0ULL && (a << b) == 0xffffdc11c09f66a0ULL && a == 0xfffffee08e04fb35ULL && b == 0x5ULL;
}

static int shr_ull(unsigned long long a, unsigned long long b)
{
    unsigned long long r = a >> b;

    return r == 0x7fffff7047027d9ULL && (a >> b) == 0x7fffff7047027d9ULL && a == 0xfffffee08e04fb35ULL && b == 0x5ULL;
}

static int add_f(float a, float b)
{
    float r = a + b;

    return r == 4.25f && (a + b) == 4.25f && a == 6.5f && b == -2.25f;
}

static int sub_f(float a, float b)
{
    float r = a - b;

    return r == 8.75f && (a - b) == 8.75f && a == 6.5f && b == -2.25f;
}

static int mul_f(float a, float b)
{
    float r = a * b;

    return r == -14.625f && (a * b) == -14.625f && a == 6.5f && b == -2.25f;
}

static int div_f(float a, float b)
{
    float r = a / b;

    return r == -2.0f && (a / b) == -2.0f && a == 6.5f && b == -3.25f;
}

int main(void)
{
    if (!add_l((-123456789L), (74565L)))
        return 1;
    if (!sub_l((-123456789L), (74565L)))
        return 2;
    if (!and_l((-123456789L), (74565L)))
        return 3;
    if (!or_l((-123456789L), (74565L)))
        return 4;
    if (!xor_l((-123456789L), (74565L)))
        return 5;
    if (!mul_l((-123456789L), (74565L)))
        return 6;
    if (!div_l((-123456789L), (74565L)))
        return 7;
    if (!rem_l((-123456789L), (74565L)))
        return 8;
    if (!shl_l((-123456789L), (5L)))
        return 9;
    if (!shr_l((-123456789L), (5L)))
        return 10;
    if (!add_ul(0xf8a432ebUL, 0x12345UL))
        return 11;
    if (!sub_ul(0xf8a432ebUL, 0x12345UL))
        return 12;
    if (!and_ul(0xf8a432ebUL, 0x12345UL))
        return 13;
    if (!or_ul(0xf8a432ebUL, 0x12345UL))
        return 14;
    if (!xor_ul(0xf8a432ebUL, 0x12345UL))
        return 15;
    if (!mul_ul(0xf8a432ebUL, 0x12345UL))
        return 16;
    if (!div_ul(0xf8a432ebUL, 0x12345UL))
        return 17;
    if (!rem_ul(0xf8a432ebUL, 0x12345UL))
        return 18;
    if (!shl_ul(0xf8a432ebUL, 0x5UL))
        return 19;
    if (!shr_ul(0xf8a432ebUL, 0x5UL))
        return 20;
    if (!add_ll((-1234567890123LL), (4886718345LL)))
        return 21;
    if (!sub_ll((-1234567890123LL), (4886718345LL)))
        return 22;
    if (!and_ll((-1234567890123LL), (4886718345LL)))
        return 23;
    if (!or_ll((-1234567890123LL), (4886718345LL)))
        return 24;
    if (!xor_ll((-1234567890123LL), (4886718345LL)))
        return 25;
    if (!mul_ll((-1234567890123LL), (4886718345LL)))
        return 26;
    if (!div_ll((-1234567890123LL), (4886718345LL)))
        return 27;
    if (!rem_ll((-1234567890123LL), (4886718345LL)))
        return 28;
    if (!shl_ll((-1234567890123LL), (5LL)))
        return 29;
    if (!shr_ll((-1234567890123LL), (5LL)))
        return 30;
    if (!add_ull(0xfffffee08e04fb35ULL, 0x123456789ULL))
        return 31;
    if (!sub_ull(0xfffffee08e04fb35ULL, 0x123456789ULL))
        return 32;
    if (!and_ull(0xfffffee08e04fb35ULL, 0x123456789ULL))
        return 33;
    if (!or_ull(0xfffffee08e04fb35ULL, 0x123456789ULL))
        return 34;
    if (!xor_ull(0xfffffee08e04fb35ULL, 0x123456789ULL))
        return 35;
    if (!mul_ull(0xfffffee08e04fb35ULL, 0x123456789ULL))
        return 36;
    if (!div_ull(0xfffffee08e04fb35ULL, 0x123456789ULL))
        return 37;
    if (!rem_ull(0xfffffee08e04fb35ULL, 0x123456789ULL))
        return 38;
    if (!shl_ull(0xfffffee08e04fb35ULL, 0x5ULL))
        return 39;
    if (!shr_ull(0xfffffee08e04fb35ULL, 0x5ULL))
        return 40;
    if (!add_f(6.5f, -2.25f))
        return 41;
    if (!sub_f(6.5f, -2.25f))
        return 42;
    if (!mul_f(6.5f, -2.25f))
        return 43;
    if (!div_f(6.5f, -3.25f))
        return 44;

    return 42;
}
