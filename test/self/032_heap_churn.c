/* The heap churned: blocks of many sizes made, grown, let go and made
 * again in an order a seed decides, each filled with its own pattern and
 * checked before it is let go or grown -- what the free list and growing
 * in place have to keep straight. And a block grown into free room after
 * it stays where it was. */
#include <stdlib.h>
#include <string.h>

#define SLOTS 64

static unsigned char *slot[SLOTS];
static unsigned len[SLOTS];
static unsigned long seed = 12345;

static unsigned next(unsigned n)
{
    seed = seed * 1103515245UL + 12345UL;

    return (unsigned) (seed >> 16) % n;
}

static int intact(int k)
{
    unsigned i;

    for (i = 0; i < len[k]; i++)
        if (slot[k][i] != (unsigned char) (k * 7 + i))
            return 0;

    return 1;
}

static void fill(int k, unsigned from)
{
    unsigned i;

    for (i = from; i < len[k]; i++)
        slot[k][i] = (unsigned char) (k * 7 + i);
}

int main(void)
{
    int step, k, bad = 0;
    unsigned char *a, *b, *grown;

    for (step = 0; step < 3000; step++) {
        k = (int) next(SLOTS);
        if (slot[k] && !intact(k))
            bad++;
        switch (next(3)) {
        case 0:
            free(slot[k]);
            slot[k] = NULL;
            len[k] = 0;
            break;
        case 1:
            free(slot[k]);
            len[k] = 1 + next(300);
            slot[k] = malloc(len[k]);
            if (!slot[k])
                return 1;
            fill(k, 0);
            break;
        default: {
            unsigned old = len[k];

            len[k] = old + 1 + next(200);
            slot[k] = realloc(slot[k], len[k]);
            if (!slot[k])
                return 2;
            fill(k, old);
            break;
        }
        }
    }
    for (k = 0; k < SLOTS; k++) {
        if (slot[k] && !intact(k))
            bad++;
        free(slot[k]);
    }
    if (bad)
        return 3;

    /* All of it free again, and one block: as big as the heap had. */
    a = malloc(20000);
    if (!a)
        return 4;
    free(a);

    /* Grown into the free room after it: where it was, what it held. */
    a = malloc(40);
    b = malloc(40);
    memset(a, 9, 40);
    free(b);
    grown = realloc(a, 70);
    if (grown != a || grown[39] != 9)
        return 5;
    free(grown);

    return 42;
}
