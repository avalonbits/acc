/* expect: 4: error: this function returns a struct, and 'return' has to give it one of the same type */
struct point { int x; };
struct point f(void) {
    return 1;
}
int main(void) {
    return 0;
}
