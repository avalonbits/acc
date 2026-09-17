/* expect: 3: error: 'later' returns a one-byte type and is called before it is defined */
int main(void) {
    return later(1);
}

char later(int n) {
    return n;
}
