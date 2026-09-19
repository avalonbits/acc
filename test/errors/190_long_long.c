/* expect: 3: error: 'long long long' is not a type */
int main(void) {
    long long long n = 1;
    return (int) n;
}
