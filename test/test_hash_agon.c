/* src/hash.s against the C it was written from, src/hash.h, on the Agon:
 * every one-character name, then thousands of names of every length from
 * none to 600 -- past a run of 256 characters, and past two -- made from a
 * seed, each byte of 1 to 255. A hash that differs still compiles a right
 * program, only a slower one, so nothing else would notice. test/hash.sh
 * builds this with agondev and runs it. */
#include <stdio.h>

#include "hash.h"

unsigned name_hash(const char *text, unsigned n);

static unsigned long seed = 12345;

static unsigned char next_byte(void)
{
    seed = seed * 1103515245UL + 12345UL;

    return (unsigned char) ((seed >> 16) % 255 + 1);
}

int main(void)
{
    static char text[600];
    unsigned checks = 0, failed = 0, n, k;

    for (n = 0; n < 256; n++) {
        text[0] = (char) n;
        checks++;
        if (name_hash(text, 1) != name_hash_c(text, 1))
            failed++;
    }
    for (k = 0; k < 3000; k++) {
        unsigned i;

        n = k < 601 ? k : next_byte() % 40;
        for (i = 0; i < n; i++)
            text[i] = (char) next_byte();
        checks++;
        if (name_hash(text, n) != name_hash_c(text, n)) {
            if (failed < 5)
                printf("FAIL length %u: %06x, %06x\r\n", n, name_hash(text, n),
                       name_hash_c(text, n));
            failed++;
        }
    }
    printf("hash: %u checks, %u failed\r\n", checks, failed);

    return 0;
}
