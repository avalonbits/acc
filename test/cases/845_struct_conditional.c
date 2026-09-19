/* ?: whose operands are structs: the chosen one, as a value -- assigned,
 * passed, returned, and read a member of. */
struct pair { int a; char b; };

struct pair make(int a) {
    struct pair p = { a, 'm' };

    return p;
}

int sum(struct pair p) {
    return p.a + p.b;
}

struct pair pick(int which, struct pair x, struct pair y) {
    return which ? x : y;
}

int main(void) {
    int r = 0;
    struct pair one = { 1, 'x' }, two = { 2, 'y' }, got;

    got = r ? one : two;
    if (got.a == 2 && got.b == 'y') r++;
    got = r ? one : two;
    if (got.a == 1) r++;
    if ((r ? make(5) : one).a == 5) r++;
    if (sum(r > 10 ? one : two) == 2 + 'y') r++;
    if (pick(0, one, two).b == 'y' && pick(1, one, two).b == 'x') r++;

    return r + 37;          /* 5 checks */
}
