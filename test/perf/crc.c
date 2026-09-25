/* Checksums: CRC-32 by table and CRC-16 by bits over an 8 KB buffer, and
 * Adler-32 over it. Unsigned 32-bit arithmetic -- a long here, wider than
 * a register -- shifts, masks and table lookups. */
#include "perf.h"

#define SIZE 8192

static unsigned char buffer[SIZE];
static unsigned long table[256];

static void make_table(void)
{
    for (unsigned n = 0; n < 256; n++) {
        unsigned long c = n;

        for (int k = 0; k < 8; k++)
            c = c & 1 ? 0xedb88320UL ^ (c >> 1) : c >> 1;
        table[n] = c;
    }
}

static unsigned long crc32(const unsigned char *p, unsigned n)
{
    unsigned long c = 0xffffffffUL;

    while (n--)
        c = table[(c ^ *p++) & 0xff] ^ (c >> 8);

    return c ^ 0xffffffffUL;
}

static unsigned crc16(const unsigned char *p, unsigned n)
{
    unsigned short c = 0xffff;

    while (n--) {
        c ^= (unsigned short) (*p++ << 8);
        for (int k = 0; k < 8; k++)
            c = c & 0x8000 ? (unsigned short) (c << 1) ^ 0x1021 : (unsigned short) (c << 1);
    }

    return c;
}

static unsigned long adler32(const unsigned char *p, unsigned n)
{
    unsigned long a = 1, b = 0;

    while (n--) {
        a = (a + *p++) % 65521UL;
        b = (b + a) % 65521UL;
    }

    return b << 16 | a;
}

int main(void)
{
    unsigned long state = perf_seed, check;

    for (unsigned i = 0; i < SIZE; i++) {
        state = state * 1664525UL + 1013904223UL;
        buffer[i] = (unsigned char) (state >> 24);
    }

    perf_start();
    make_table();
    check = crc32(buffer, SIZE);
    check ^= crc16(buffer, SIZE);
    check += adler32(buffer, SIZE);
    perf_stop();

    perf_check(check);

    return 0;
}
