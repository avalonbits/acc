/* expect: 3:14: error: 'T' is already declared */
typedef int T;
typedef char T;
int main(void) {
    return 0;
}
