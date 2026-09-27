/* expect: 5:16: error: 'e' is already declared */
int e;
int main(void) {
    static int e;
    extern int e;
    return e;
}
