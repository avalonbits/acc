/* expect: 7: error: the operator '*' is not implemented for float yet */
/* Adding and subtracting floats is done; multiplying and dividing them is
 * not, and the two are separate routines rather than one. */
int main(void) {
    float a = 1.5;
    float b = 2.5;
    float c = a * b;
    return 0;
}
