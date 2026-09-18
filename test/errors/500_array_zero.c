/* expect: 4: error: an array needs at least one element */
/* C has no empty arrays. */
int main(void) {
    char a[0];
    return 0;
}
