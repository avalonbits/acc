/* expect: 4: error: 'c' is already declared */
int main(void) {
    char c = 1;
    int c = 2;
    return c;
}
