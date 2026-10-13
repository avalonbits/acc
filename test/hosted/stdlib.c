/* <stdlib.h>'s heap where the answer is a rule rather than an address:
 * calloc refuses a count times a size past what a size_t holds, which on
 * the Agon is 24 bits and on the host 64, rather than give a block the
 * product wrapped round to; and what it does give is cleared. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    volatile size_t half = SIZE_MAX / 2 + 2;
    unsigned char *p;
    int k, clear = 1;

    printf("calloc past size_t %d\n", calloc(half, 2) == NULL);
    printf("calloc past size_t, other way %d\n", calloc(2, half) == NULL);
    p = calloc(10, 3);
    for (k = 0; p && k < 30; k++)
        clear &= p[k] == 0;
    printf("calloc 10 3 %d clear %d\n", p != NULL, clear);
    free(p);

    return 0;
}
