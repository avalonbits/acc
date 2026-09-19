/* Pointers to functions: declared plainly, in arrays, in structs, through
 * typedefs and as parameters; set from a function's name or its address,
 * some before the function is defined; and called every way C allows. */
typedef int (*unary)(int);
typedef int binary(int, int);

int twice(int x);
int square(int x);
int add(int a, int b) { return a + b; }
long wide(long x) { return x * 2; }
char first(const char *s) { return s[0]; }
void touch(int *p) { *p = 7; }

struct ops { unary one; binary *two; const char *name; };

unary table[3] = { twice, square, &twice };
struct ops global_ops = { square, add, "g" };

int apply(int (*f)(int), int x) {
    return f(x) + (*f)(x);
}

int fold(binary *f, int n) {
    int acc = 0;

    for (int i = 1; i <= n; i++)
        acc = f(acc, i);

    return acc;
}

int (*pick(int which))(int) {
    return which ? square : twice;
}

unary pick_by_name(char c) {
    return c == 's' ? square : twice;
}

int main(void) {
    int r = 0;
    int (*fp)(int) = twice;
    long (*wp)(long) = &wide;
    char (*cp)(const char *) = first;
    void (*vp)(int *) = touch;
    int (**pp)(int) = &fp;
    struct ops local = { twice, &add, "l" };
    struct ops *op = &local;
    int n = 0;

    if (fp(3) == 6 && (*fp)(4) == 8 && (**pp)(5) == 10) r++;
    if (wp(100000) == 200000 && cp("xyz") == 'x') r++;
    vp(&n);
    if (n == 7) r++;
    if (table[0](2) + table[1](3) + table[2](4) == 4 + 9 + 8) r++;
    if (global_ops.one(5) == 25 && global_ops.two(2, 3) == 5) r++;
    if (op->one(6) == 12 && (*op->two)(4, 4) == 8 && op->name[0] == 'l') r++;
    if (apply(square, 3) == 18 && apply(twice, 3) == 12) r++;
    if (fold(add, 4) == 10 && fold(&add, 5) == 15) r++;
    if (pick(1)(4) == 16 && pick(0)(4) == 8 && pick_by_name('s')(2) == 4) r++;
    fp = pick(1);
    if (fp == square && fp != twice && fp && table[0] == table[2]) r++;
    if (sizeof fp == 3 && sizeof(int (*)(int)) == 3) r++;

    return r + 31;          /* 11 checks */
}

int twice(int x) {
    return 2 * x;
}

int square(int x) {
    return x * x;
}
