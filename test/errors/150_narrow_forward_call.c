/* expect: 3: error: 'later' is called and not declared */
int main(void) {
    return later(1);
}

char later(int n) {
    return n;
}
