/* expect: 4: error: 'f' takes 2 arguments, and this call gives it 1 */
int f(int a, int b);
int main(void) {
    return f(1);
}
int f(int a, int b) {
    return a + b;
}
