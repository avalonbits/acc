/* expect: 5:16: error: 'missing' is not declared */
#define INNER (missing + 1)
#define OUTER(v) ((v) * INNER)
int main(void) {
    return 2 + OUTER(3);
}
