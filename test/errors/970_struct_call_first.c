/* expect: 5: error: 'f' returns a struct, so it has to be defined before it is called */
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
