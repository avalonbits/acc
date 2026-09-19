/* expect: 4: error: '\q' is not an escape C has */
/* Only the escapes C defines. */
int main(void) {
    return '\q';
}
