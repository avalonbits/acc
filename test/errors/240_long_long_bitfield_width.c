/* expect: 2: error: a bit-field of this type is 0 to 64 bits wide */
struct s { long long a : 65; };
int main(void) {
    return 0;
}
