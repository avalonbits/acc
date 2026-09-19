/* expect: 5: error: this is const, so it cannot be changed */
int x = 1;
int main(void) {
    const int *p = &x;
    *p = 2;
    return x;
}
