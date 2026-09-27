/* expect: 7:9: error: hex constant with no digits */
/* An error from inside the number lexer, which reports at the line the token
 * began on.
 */
int main(void) {
    int a =
        0x;
    return a;
}
