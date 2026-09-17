/* expect: 3: error: the constant is too large for a long */
int main(void) {
    long n = 3000000000;
    return n;
}
