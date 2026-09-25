/* << by a constant of up to eight, written out as doublings rather than
 * called for: wrapping at 24 bits, negative ints shifted as the unsigned
 * they convert to -- shifting a negative int left is undefined -- a count
 * of 0 and of 8, and in an expression as crc16 has them. */
static unsigned values[] = { 1u, 0x400000u, 0x123456u, 0xffffffu, 0x0000ffu };
static int signeds[] = { -3, -1, 7, -8388608 };

int main(void)
{
    unsigned long sum = 0;

    for (int i = 0; i < 5; i++) {
        unsigned v = values[i];

        sum += v << 0;
        sum += v << 1;
        sum += v << 2;
        sum += v << 7;
        sum += v << 8;
        sum += (unsigned short) (v << 1) ^ 0x1021u;
    }
    for (int i = 0; i < 4; i++)
        sum += ((unsigned) signeds[i] << 4) + ((unsigned) signeds[i] << 8);

    /* From a model of the shifts at 24 bits, in Python. */
    return sum == 143725202UL ? 42 : 1;
}
