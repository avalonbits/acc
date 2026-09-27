/* expect: 3:28: error: the constant does not fit in 64 bits */
int main(void) {
    unsigned long long n = 0x10000000000000000;
    return (int) n;
}
