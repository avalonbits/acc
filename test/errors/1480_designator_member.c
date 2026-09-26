/* expect: 4:24: error: 'struct point' has no member 'z' */
struct point { int x, y; };
int main(void) {
    struct point p = { .z = 1 };
    return p.x;
}
