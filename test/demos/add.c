/* Functions, parameters, calls. Prints 00002A. */
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

int main(void) {
    int x = 30;
    int y = 20;
    return add(x, y) - sub(10, 2);
}
