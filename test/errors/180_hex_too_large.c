/* expect: 3: error: the constant does not fit in 24 bits */
int main(void) {
    int n = 0x1000000;
    return n;
}
