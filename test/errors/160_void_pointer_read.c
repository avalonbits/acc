/* expect: 8:14: error: a 'void *' does not say what it points at, so it cannot be read through */
/* A void * holds an address and nothing else: there is no width to read
 * through it and no step to walk it by. Both are refused where they are
 * written rather than guessed at. */
int main(void) {
    int n = 1;
    void *p = &n;
    return *p;
}
