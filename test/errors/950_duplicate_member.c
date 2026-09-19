/* expect: 2: error: 'x' is already a member of 'struct point' */
struct point { int x; char x; };
int main(void) {
    return 0;
}
