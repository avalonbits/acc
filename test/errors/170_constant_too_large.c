/* expect: 3: error: the constant does not fit in 64 bits */
int main(void) {
    unsigned long long n = 99999999999999999999999;
    return (int) n;
}
