/* expect: 4:15: error: 'x' is declared register, so it has no address to take */
int main(void) {
    register int x = 1;
    int *p = &x;
    return *p;
}
