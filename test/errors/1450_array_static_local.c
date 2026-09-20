/* expect: 3: error: 'static' and qualifiers inside [] say something about a parameter, and this is not one */
int main(void) {
    int a[static 3];
    return a[0];
}
