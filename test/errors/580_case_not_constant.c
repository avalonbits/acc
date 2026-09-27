/* expect: 6:10: error: a case label has to be a constant integer */
/* A case is a constant, fixed when the switch is compiled. */
int main(void) {
    int n = 2;
    switch (n) {
    case n:
        return 1;
    }
    return 0;
}
