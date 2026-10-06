/* A function's address as a value, in functions opt-acc's machine-level
 * backend makes: the constant where the function is defined already,
 * moved as the link moves the image; a load the link fills in where it is
 * defined further down; a static reached only through its address, made
 * all the same. Called through, stored in a table, compared. */
typedef int (*binop)(int, int);

static int add(int a, int b) { return a + b; }
static int later(int a, int b);
static int only_by_address(int a, int b) { return a * b; }

static binop table[3];

static int apply(binop f, int a, int b) { return f(a, b); }

static int pick(int k)
{
    return apply(k ? add : later, 6, 4) + apply(only_by_address, 2, 3);
}

static void fill(void)
{
    table[0] = add;
    table[1] = later;
    table[2] = only_by_address;
}

static int later(int a, int b) { return a - b; }

int main(void)
{
    int ok = 0;

    fill();
    ok += pick(1) == 10 + 6 && pick(0) == 2 + 6;
    ok += table[0](1, 2) == 3 && table[1](5, 2) == 3 && table[2](4, 5) == 20;
    ok += table[1] == later && table[0] != table[2];

    return ok * 14;
}
