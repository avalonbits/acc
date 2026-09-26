/* expect: 5:12: error: this is const, so it cannot be changed */
/* The address of a const variable points at something const. */
int main(void) {
    const int n = 1;
    *&n = 3;
    return n;
}
