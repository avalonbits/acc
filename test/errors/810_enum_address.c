/* expect: 4:15: error: 'A' is a constant, which has no address */
enum { A = 1 };
int main(void) {
    int *p = &A;
    return *p;
}
