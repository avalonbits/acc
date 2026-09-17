/* expect: 3: error: the constant does not fit in 32 bits */
int main(void) {
    long n = 0x100000000;
    return n;
}
