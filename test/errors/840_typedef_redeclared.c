/* expect: 3: error: 'T' is already declared */
typedef int T;
typedef char T;
int main(void) {
    return 0;
}
