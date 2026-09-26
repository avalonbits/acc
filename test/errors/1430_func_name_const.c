/* expect: 3:22: error: this is const, so it cannot be changed */
int main(void) {
    __func__[0] = 'x';
    return 0;
}
