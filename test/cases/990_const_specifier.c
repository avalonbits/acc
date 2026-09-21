/* What is const after a struct, union or enum body has been read.
 *
 * The members of a struct are declarations, and they go through the same
 * reader the declaration around them does -- so the last member's const was
 * still marked when that reader came back, and it was taken for the const of
 * whatever that declaration was declaring. A struct whose last member is a
 * `const char *` made a const of the variable declared with the body, and a
 * typedef of it made a const of everything declared with the typedef. Every
 * store to one was refused, which is a compile error and not a wrong answer:
 * this case fails by not building. */

/* The body and the variable in one declaration. */
struct {
    int count;
    const char *label;          /* last, and const at the bottom */
} declared;

/* And through a typedef, which is how zap's tables are written. */
typedef struct {
    int count;
    const int *numbers;
} boxed;

typedef union {
    int number;
    const char *text;
} either;

/* An enum's constants are expressions, and one of them can name a type. */
enum bits { CHARS = sizeof(const char), LONGS = sizeof(const long) } flags;

static const int table[3] = { 7, 8, 9 };

/* The const a declaration really does ask for still has to be kept: this one
 * is written once, here, and never again. */
static const boxed frozen = { 4, table };

int main(void)
{
    boxed b;
    either e;
    int total = 0;

    declared.count = 3;
    declared.label = "three";
    b.count = 2;
    b.numbers = table;
    e.number = 6;
    flags = LONGS;

    total += declared.count;             /* 3 */
    total += declared.label[0] - 't' + 1;/* 1 */
    total += b.count;                    /* 2 */
    total += b.numbers[2];               /* 9 */
    total += e.number;                   /* 6 */
    total += (int) flags;                /* 4 */
    total += frozen.count;               /* 4 */
    total += frozen.numbers[1];          /* 8 */
    total += CHARS;                      /* 1 */
    total += 4;

    return total;                        /* 42 */
}
