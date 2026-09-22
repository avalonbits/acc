/* expect: 4: error: '&' takes the address of a variable, and a cast has none */
int main(void) {
    int x = 1;
    int *p = &(int) x;

    return p != 0;
}
