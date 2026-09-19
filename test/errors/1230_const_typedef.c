/* expect: 6: error: this is const, so it cannot be changed */
/* A typedef keeps the const it was declared with. */
typedef const int cint;
int x;
int main(void) { cint *p = &x;
    *p = 1;
    return 0;
}
