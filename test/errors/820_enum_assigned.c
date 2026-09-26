/* expect: 4:7: error: the left of '=' is not something that can be assigned to */
enum { A = 1 };
int main(void) {
    A = 2;
    return A;
}
