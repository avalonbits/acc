/* expect: 4:14: error: 'n' cannot be changed by '++' */
int main(void) {
    const int n = 1;
    return ++n;
}
