/* expect: 2:16: error: a bit-field of no bits cannot have a name */
struct s { int a : 0; };
int main(void) {
    return 0;
}
