/* The operators opt-acc's own backend makes now. A byte's + - & | ^ << >>,
 * where only the answer's byte is kept, is made in A -- a subtraction with
 * the right side on the stack as -right + left. A multiply by a constant is
 * doublings and additions; any other multiply, divide, remainder or shift
 * is the first pass's helper, BC kept. Each check is on its own. */
static unsigned char bytes(unsigned char a, unsigned char b, unsigned char c)
{
    unsigned char t = a + b;

    t = t - c;
    t = (unsigned char) (t ^ 0x0f);
    t = (unsigned char) (t << 2);
    t = t | (unsigned char) (c >> 1);

    return t;
}

/* The right side made on the stack, and taken from a byte made before it. */
static unsigned char minus_made(unsigned char a, unsigned char b)
{
    unsigned char t = (unsigned char) (a - (unsigned char) (b + 3));

    return t;
}

/* The right side made first on the stack: -right + left. */
static unsigned char minus_first(unsigned char a, unsigned char b, unsigned char c)
{
    unsigned char t = a - (b & c);

    return t;
}

static int multiply(int x)
{
    return x * 10 + x * 8 + x * 13 + x * 0x7fff;
}

static int arithmetic(int a, int b, unsigned int u, unsigned int v, int n)
{
    int right = 0;

    right += a * b == -42;
    right += a / b == -1 && a % b == -1;      /* -7 / 6, -7 % 6 */
    right += u / v == 699050 && u % v == 5;
    right += (a << n) == -56 && (u >> n) == 2097150;
    right += (a >> n) == -1 && (a << 12) == -28672;
    right += (b >> 1) == 3;

    return right;
}

int main(void)
{
    int right = 0;

    /* (5 + 9 - 3) = 11, ^ 15 = 4, << 2 = 16, | (3 >> 1 = 1) = 17 */
    right += bytes(5, 9, 3) == 17;
    right += bytes(250, 9, 3) == (unsigned char) (((((250 + 9 - 3) & 0xff) ^ 15) << 2) | 1);
    right += minus_made(10, 4) == 3 && minus_made(1, 4) == 250;
    right += minus_first(10, 7, 3) == 7 && minus_first(1, 6, 6) == 251;
    right += multiply(3) == 30 + 24 + 39 + 98301;
    right += arithmetic(-7, 6, 0xfffff5, 24, 3) == 6;
    return right == 6 ? 42 : right;
}
