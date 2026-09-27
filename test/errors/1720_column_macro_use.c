/* expect: 6:12: error: 'missing' is not declared */
/* An error inside a macro's text is where the macro was used. */
#define USE(v) ((v) + missing)
int main(void) {
    int x = 1;
    return USE(x);
}
