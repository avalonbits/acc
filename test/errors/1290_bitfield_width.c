/* expect: 2:17: error: a bit-field of this type is 0 to 8 bits wide */
struct s { char c : 9; };
int main(void) {
    return 0;
}
