/* Long long multiply, divide and remainder, signed and unsigned, over
 * operands of every length.
 *
 * The runtime's divide keeps the remainder in registers, compares against
 * the divisor from its top limb down, and skips the dividend's leading zero
 * bytes; its multiply adds byte products a row at a time and skips a row
 * whose byte is zero. Each of those is a path a value of the wrong shape
 * would miss, so the operands here have from none to eight significant
 * bytes, and some are all ones or have the top bit set.
 *
 * Every answer is folded into a hash, and the hash is what the host's own
 * 64-bit arithmetic made of the same operations. */
typedef unsigned long long u64;
typedef long long s64;

static u64 seed = 0x2545f4914f6cdd1dULL;

/* A value of a random number of significant bytes, so that every length
 * the divide skips to is reached, and now and then all ones or a top bit. */
static u64 value(void)
{
    u64 v;
    int bytes;

    seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
    v = seed ^ seed >> 29;
    bytes = (int) (seed >> 58 & 15);
    if (bytes >= 8)
        return bytes == 15 ? ~0ULL : bytes == 14 ? v | 1ULL << 63 : v;
    if (bytes == 0)
        return 0;

    return v >> (64 - 8 * bytes);
}

static u64 hash = 0;

static void mix(u64 v)
{
    hash = (hash ^ v) * 1099511628211ULL;
}

int main(void)
{
    int i;
    u64 x, y;
    s64 a, b;

    for (i = 0; i < 1500; i++) {
        x = value();
        y = value();
        mix(x * y);
        if (y != 0) {
            mix(x / y);
            mix(x % y);
        }
        a = (s64) x;
        b = (s64) y;
        if (b != 0 && !(a == (s64) (1ULL << 63) && b == -1)) {
            mix((u64) (a / b));
            mix((u64) (a % b));
        }
    }
    return hash == 0x777e5fbdf29d8150ULL ? 42 : 1;
}
