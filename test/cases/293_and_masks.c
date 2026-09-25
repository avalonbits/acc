/* & with a constant that keeps something of the middle byte and nothing of
 * the top one -- 0x8000, 0xffff, 0x0ff0, 0x1234 -- written out rather than
 * called for, on values with bits set in all three bytes, signed and not,
 * and in a test as crc16 has it. */
static unsigned values[] = { 0xffffffu, 0x800000u, 0x7fffffu, 0xabcdefu,
                             0x123456u, 0x00ff00u, 0x0000ffu, 0u };
static int negatives[] = { -1, -2, -32768, -65536, -8388608 };

int main(void)
{
    unsigned long sum = 0;
    int bits = 0;

    for (int i = 0; i < 8; i++) {
        unsigned v = values[i];

        sum += v & 0x8000;
        sum += v & 0xffff;
        sum += v & 0x0ff0;
        sum += v & 0x1234;
        if (v & 0x8000)
            bits++;
    }
    for (int i = 0; i < 5; i++) {
        int n = negatives[i];

        sum += (unsigned) (n & 0xffff) + (unsigned) (n & 0x8000)
               + (unsigned) (n & 0x0ff0);
    }

    /* 699,123 and four of the eight with bit 15 set, from a model of the
     * masks in Python. */
    return sum == 699123UL && bits == 4 ? 42 : 1;
}
