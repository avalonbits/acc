/* expect: 4:12: error: '\q' is not an escape C has */
/* Only the escapes C defines. */
int main(void) {
    return '\q';
}
