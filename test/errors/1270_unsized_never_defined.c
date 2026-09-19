/* expect: error: 'a' is declared with no size and never defined */
extern int a[];
int main(void) {
    return a[0];
}
