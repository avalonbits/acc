/* Structs and arrays as values in opt-acc's own backend: each is its
 * address. A struct read through a pointer is the address, and one written
 * through a pointer is its bytes copied, ldir with BC kept -- a value may
 * live in BC; an array read through a pointer, a member array, is its
 * address too. Each check is on its own. */
struct operand { int mode; unsigned char reg; char name[5]; int disp; };

static const struct operand none = { 0, 0xff, "none", -1 };

static void reset(struct operand *op)
{
    *op = none;
}

static void copy_to(struct operand *to, const struct operand *from)
{
    *to = *from;
}

/* `n` lives in BC across the copies: the ldir must give it back. */
static int copy_many(struct operand *to, const struct operand *from, int n)
{
    int total = 0;

    while (n--) {
        *to++ = *from;
        total += n;
    }

    return total;
}

static int name_char(const struct operand *op, int at)
{
    const char *name = op->name;

    return name[at];
}

static int row_char(char (*rows)[4], int row, int at)
{
    return rows[row][at];
}

int main(void)
{
    struct operand a, b[3];
    char rows[2][4] = { "abc", "xyz" };
    int right = 0;

    a.mode = 7;
    reset(&a);
    right += a.mode == 0 && a.reg == 0xff && a.disp == -1 && a.name[3] == 'e';
    a.mode = 3;
    a.disp = 1234;
    copy_to(&b[1], &a);
    right += b[1].mode == 3 && b[1].disp == 1234 && b[1].name[0] == 'n';
    right += copy_many(b, &a, 3) == 2 + 1 + 0 && b[2].disp == 1234;
    right += name_char(&a, 1) == 'o';
    right += row_char(rows, 1, 2) == 'z';
    return right == 5 ? 42 : right;
}
