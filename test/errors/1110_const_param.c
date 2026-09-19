/* expect: 3: error: 'x' is const, so it cannot be changed */
int f(const int x) {
    x += 1;
    return x;
}
int main(void) {
    return f(1);
}
