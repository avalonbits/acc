/* expect: 4: error: 'f' is called and not declared */
/* The call took the result to be an int, which cannot be put right after. */
struct point { int x; };
int g(void) { return f(); }
struct point f(void) {
    struct point p = { 1 };
    return p;
}
int main(void) {
    return 0;
}
