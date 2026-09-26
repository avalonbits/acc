/* expect: 4:10: error: expected '(', found a name */
int main(void) {
    int n = 0;
    while n
        n = n - 1;
    return n;
}
