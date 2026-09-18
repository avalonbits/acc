/* expect: 4: error: '*' takes a pointer, and this is an integer */
int main(void) {
    int n = 1;
    return *n;
}
