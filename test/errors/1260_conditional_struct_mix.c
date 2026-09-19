/* expect: 6: error: the two sides of ?: have to be the same struct or union, or neither be one */
struct a { int x; };
struct b { int x; };
int main(void) {
    struct a s; struct b t;
    return (1 ? s : t).x;
}
