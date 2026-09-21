/* __builtin_offsetof, which is what <stddef.h>'s offsetof is.
 *
 * The usual definition takes the address of a member through a null pointer,
 * and acc refuses that everywhere else -- so the offset is worked out by the
 * compiler instead, from a designator that can walk members and subscripts
 * the way an initialiser's does. */

struct inner { char tag; int value; };

struct outer {
    char        head;
    struct inner rows[3];
    int         tail;
    char        name[4];
};

union either { int number; char bytes[4]; };

int main(void)
{
    int total = 0;

    total += (int) __builtin_offsetof(struct outer, head);        /* 0 */
    total += (int) __builtin_offsetof(struct outer, rows);        /* 1 */
    total += (int) __builtin_offsetof(struct outer, rows[1]);     /* 5 */
    total += (int) __builtin_offsetof(struct outer, rows[2].value);
                                                                  /* 10 */
    total += (int) __builtin_offsetof(struct outer, tail);        /* 13 */
    total += (int) __builtin_offsetof(struct outer, name[2]);     /* 18 */
    total += (int) __builtin_offsetof(union either, bytes[3]);    /* 3 */
    total += (int) sizeof(struct outer) - 20;                     /* 0 */

    /* It is a constant expression: it can size an array. */
    {
        char room[__builtin_offsetof(struct outer, tail)];

        total += (int) sizeof room - 13;                          /* 0 */
    }

    return total - 8;            /* 42 */
}
