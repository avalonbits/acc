/* What agondev's linker and library give a program about its memory, as
 * Berry's port asks it: __heapbot where the program's own memory ends,
 * __heaptop the top of all of it, and sbrk(0) the break -- a block from
 * malloc between the two ends, and the break no higher than the top. (Not
 * the stack's place: the oracle's startup leaves it on MOS's.) */
#include <stdlib.h>

extern char __heapbot, __heaptop;
extern void *sbrk(int);

static char kept[16];

int main(void)
{
    char *block = malloc(32);
    char *brk = sbrk(0);

    if (!block)
        return 1;
    if (!(&__heapbot < &__heaptop) || !(kept < &__heapbot + 1))
        return 2;
    if (!(block >= &__heapbot && block + 32 <= brk))
        return 3;
    if (!(brk <= &__heaptop))
        return 4;
    free(block);

    return 42;
}
