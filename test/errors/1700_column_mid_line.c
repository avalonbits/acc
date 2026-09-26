/* expect: 3:17: error: 'nope' is not declared */
int main(void) {
    int a = 1 + nope * 2;
    return a;
}
