/* expect: 4: error: 'B' is already declared */
/* Enum constants share the scope of ordinary names. */
enum first { A, B };
enum second { B, C };
int main(void) {
    return A;
}
