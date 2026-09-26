/* expect: 5:17: error: a struct or union cannot be used as a number */
struct point { int x, y; };
int main(void) {
    struct point p = { 1, 2 };
    return p + 1;
}
