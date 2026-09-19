/* expect: 6: error: this is const, so it cannot be changed */
int x;
int main(void) {
    const int *p = &x;
    const int **pp = &p;
    **pp = 1;
    return 0;
}
