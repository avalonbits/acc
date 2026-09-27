/* expect: 4:12: error: this function takes 2 arguments, and this call gives it 1 */
int add(int a, int b) { return a + b; }
int main(void) { int (*p)(int, int) = add;
    return p(1);
}
