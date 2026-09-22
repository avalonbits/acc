/* expect: 3: error: '&&' takes the address of a label, and this is a number */
int main(void) {
    void *p = &&3;

    return p != 0;
}
