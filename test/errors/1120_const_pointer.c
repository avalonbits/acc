/* expect: 5:7: error: 'p' is const, so it cannot be changed */
/* The pointer is const here, not what it points at. */
int main(void) {
    int a = 1, b = 2, *const p = &a;
    p = &b;
    return *p;
}
