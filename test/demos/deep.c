/* Calls whose arguments are calls. Prints 00002A. */
int f(int a, int b) { return a + b; }

int main(void) {
    return f(f(f(1, 2), f(3, 4)), f(f(5, 6), f(7, 8))) + 6;
}
