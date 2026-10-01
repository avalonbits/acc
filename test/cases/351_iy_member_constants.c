/* Constants written to the members of a struct through a pointer the leaf
 * backend keeps in IY -- ld (iy+d), n and ld (iy+d), hl -- the pointer
 * stepped by the struct's size in place, and the assignment's value used. */
typedef struct {
    unsigned char r0, r1;
    _Bool b;
    int i;
    signed char s;
} Op;

static int fill(Op *op, int n)
{
    int last = 0;

    while (n--) {
        op->r0 = 7;
        op->r1 = 200;
        op->b = 5;                  /* 1, as a _Bool */
        op->i = -2;
        last = op->s = -3;          /* the value kept */
        op++;
    }

    return last;
}

static int back(Op *op, int n)
{
    int sum = 0;

    op += n - 1;
    while (n--) {
        sum += op->i;
        op--;
    }

    return sum;
}

static Op ops[4];

int main(void)
{
    int right = 0, k;

    ops[3].i = 99;
    right += fill(ops, 3) == -3;
    for (k = 0; k < 3; k++)
        right += ops[k].r0 == 7 && ops[k].r1 == 200 && ops[k].b == 1
                 && ops[k].i == -2 && ops[k].s == -3;
    right += ops[3].i == 99 && ops[3].r0 == 0;
    right += back(ops, 4) == 93;
    return right == 6 ? 42 : right;
}
