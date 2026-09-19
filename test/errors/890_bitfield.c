/* expect: 2: error: bit-fields are not supported yet */
struct flags { int a : 3; };
int main(void) {
    return 0;
}
