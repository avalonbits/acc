/* expect: 5:18: error: 'struct later' is declared but its members are not given */
/* A pointer to it is fine; the struct itself has no size yet. */
struct later *p;
int main(void) {
    struct later l;
    return 0;
}
