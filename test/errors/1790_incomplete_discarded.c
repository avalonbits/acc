/* expect: 5: error: 'struct S' is declared but its members are not given */
struct S;
extern struct S *vp;
void f(void) {
    (*vp, (void) 0);
}
