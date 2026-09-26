/* expect: 3:1: error: 'u' is declared as a struct tag, not a union one */
struct u { int x; };
union u *p;
int main(void) {
    return 0;
}
