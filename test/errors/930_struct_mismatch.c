/* expect: 6: error: a struct can only be assigned a struct of the same type */
struct a { int x; };
struct b { int x; };
int main(void) {
    struct a s; struct b t;
    s = t;
    return 0;
}
