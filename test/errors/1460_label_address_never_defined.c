/* expect: 3: error: the label 'nowhere' is used but never defined */
int main(void) {
    void *p = &&nowhere;

    return p != 0;
}
