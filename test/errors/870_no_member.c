/* expect: 5:14: error: 'struct point' has no member 'z' */
struct point { int x, y; };
int main(void) {
    struct point p;
    return p.z;
}
