/* expect: 4:31: error: a global's initial value has to be a constant */
/* A static local is initialised once, before the program runs: from a constant. */
int main(void) {
    int n = 3; static int m = n;
    return m;
}
