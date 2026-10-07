/* A body read in place inside &&, holding another, both with locals whose
 * address is taken; the caller's own such locals after it. */
static int storage, seen;
static int toks[8] = { 1, 2, 3, 0, 4, 5, 0, 0 };
static int at;

typedef unsigned char Type;
static __attribute__((noinline)) int declarator(Type base, Type *type, int *ext, int *count)
{
    *type = base + toks[at];
    *ext = toks[at] * 2;
    *count = 1;
    at++;

    return toks[at - 1];
}

static void storage_declarators(int st, Type base, int bx, unsigned char bc)
{
    for (;;) {
        int line = at, count, ext;
        Type type, stars = (Type) (base + bx);
        int name = declarator(stars, &type, &ext, &count);

        seen += type + ext + count + line + name + st + bc;
        if (toks[at] == 0)
            break;
    }
    at++;
}

static int storage_declaration(Type base, int bx, unsigned char bc)
{
    int st = storage;

    if (st == 2) {
        storage_declarators(st, base, bx, bc);

        return 1;
    }

    return 0;
}

static int declaration(Type base)
{
    int bx = base * 3;
    unsigned char bc = (unsigned char) base;
    int sum = 0;

    if (storage && storage_declaration(base, bx, bc))
        return -1;
    for (;;) {
        int line = at, count, ext, off = 7, sym = 9, far = 0;
        Type type, stars = (Type) (base + 1);
        int name = declarator(stars, &type, &ext, &count);

        sum += type * 3 + ext + count + line + name + off + sym + far;
        if (toks[at] == 0)
            break;
    }

    return sum;
}

int main(void)
{
    int r;

    storage = 0;
    r = declaration(10);
    if (r != 189)
        return r & 0x7f;
    storage = 2;
    at = 0;
    if (declaration(5) != -1)
        return 2;

    return 42;
}
