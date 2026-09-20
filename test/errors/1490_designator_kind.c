/* expect: 4: error: '.' designates a member, and this is not a struct or a union */
struct point { int x, y; };
int main(void) {
    struct point p = { .x.y = 1 };
    return p.x;
}
