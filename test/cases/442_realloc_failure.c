/* realloc that cannot have the room: NULL, and the block left as it was
 * -- its bytes, and still the program's to grow when there is room, and
 * to free. Berry's VM collects its garbage and asks again with the same
 * pointer; it had been freed, and its stack went on in freed memory. */
#include <stdlib.h>

int main(void)
{
    unsigned char *block = malloc(100);
    unsigned char *grown;
    int k;

    if (!block)
        return 1;
    for (k = 0; k < 100; k++)
        block[k] = (unsigned char) (k * 5 + 1);
    if (realloc(block, 600000) != NULL)
        return 2;
    for (k = 0; k < 100; k++)
        if (block[k] != (unsigned char) (k * 5 + 1))
            return 3;
    grown = realloc(block, 300);
    if (!grown)
        return 4;
    for (k = 0; k < 100; k++)
        if (grown[k] != (unsigned char) (k * 5 + 1))
            return 5;
    free(grown);

    return 42;
}
