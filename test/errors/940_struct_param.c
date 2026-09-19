/* expect: 3: error: a struct or union passed by value is not supported yet */
struct point { int x; };
int f(struct point p) {
    return 0;
}
int main(void) {
    return 0;
}
