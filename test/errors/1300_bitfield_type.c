/* expect: 2:18: error: a bit-field has to have an integer type */
struct s { float f : 3; };
int main(void) {
    return 0;
}
