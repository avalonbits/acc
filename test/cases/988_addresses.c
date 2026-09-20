/* Addresses written into the image: a global's, a string's, a function's,
 * and one worked out from another -- in code and in a global's bytes.
 *
 * Every one of these is a place the compiler writes down where something is,
 * and so a place a relocation has to be recorded; test/reloc.sh compiles this
 * at two load addresses and requires them to agree. The shapes here are the
 * ones the rest of the cases do not reach: more pointers in one initialiser
 * than the old fixed sixteen, an initialiser given out of order, and a
 * variable whose value is written back over the room an earlier mention of it
 * reserved. */
int numbers[5] = { 1, 2, 3, 4, 5 };

/* Eighteen, which the table of addresses in an initialiser used to have room
 * for sixteen of. */
const char *const names[] = {
    "a", "b", "c", "d", "e", "f", "g", "h", "i",
    "j", "k", "l", "m", "n", "o", "p", "q", "r"
};

/* Out of order, so the addresses do not arrive at rising offsets. */
int *const spread[4] = { [3] = numbers + 3, [1] = numbers + 1 };

/* Said before it is given a value, with something compiled in between that
 * has addresses of its own: the value goes back where the first mention
 * reserved room for it, and what came between keeps its own. */
int *first;

int twice(int n) { return n + n; }

static int sum_spread(void)
{
    int total = 0;
    int i;

    for (i = 0; i < 4; i++)
        if (spread[i])
            total += *spread[i];

    return total;
}

int *first = numbers + 2;

int (*const doubler)(int) = twice;

int main(void)
{
    int total = 0;

    total += *first;                     /* 3 */
    total += sum_spread();               /* 2 + 4 */
    total += names[17][0] - 'a';         /* 17 */
    total += doubler(6);                 /* 12 */
    total += (&numbers[4]) - numbers;    /* 4, and not an address */

    return total;                        /* 42 */
}
