/* expect: 3:6: error: 'x' is declared again with another type */
extern int x;
long x;
int main(void) {
    return 0;
}
