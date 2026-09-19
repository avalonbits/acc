/* expect: 4: error: the label 'nowhere' is used but never defined */
/* Blamed on the goto that named it. */
int main(void) {
    goto nowhere;
    return 0;
}
