/* expect: 4:27: error: a pointer to a function has no step to take */
int f(void) { return 0; }
int main(void) {
    int (*p)(void) = f + 1;
    return p();
}
