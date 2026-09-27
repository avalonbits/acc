/* expect: 7:16: error: argument 1 has to be a struct of the parameter's type */
struct point { int x; };
int f(struct point p) {
    return p.x;
}
int main(void) {
    return f(3);
}
