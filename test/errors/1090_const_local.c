/* expect: 4:7: error: 'n' is const, so it cannot be changed */
int main(void) {
    const int n = 1;
    n = 2;
    return n;
}
