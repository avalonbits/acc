/* expect: 5: error: '8' is not an octal digit, and a constant that starts with 0 is octal */
/* Refused only once the constant is known to be an integer: `08.5` would be
 * an ordinary decimal float. */
int main(void) {
    int a = 018;
    return a;
}
