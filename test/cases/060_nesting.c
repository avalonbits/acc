int f(int a, int b, int c) { return a + b - c; }
int main(void) {
    int x = 1, y = 2, z = 3;
    return f(x + y, z - x, y - z) + f(1, 2, 3) + 35;
}
