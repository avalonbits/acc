/* expect: 5:15: error: a bit-field has no address to take */
struct s { int a : 3; };
int main(void) {
    struct s v;
    int *p = &v.a;
    return *p;
}
