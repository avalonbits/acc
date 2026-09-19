/* expect: 3: error: 'struct point' is defined twice */
struct point { int x; };
struct point { int y; };
int main(void) {
    return 0;
}
