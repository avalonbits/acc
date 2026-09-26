/* expect: 5:12: error: a bit-field has no size of its own */
struct s { int a : 3; };
int main(void) {
    struct s v;
    return sizeof v.a;
}
