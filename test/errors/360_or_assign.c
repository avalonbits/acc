/* expect: 8: error: '||=' is not an operator in C; `a = a || b` is what it would mean */
/* Refused by name, as '&&=' is: lexed as `||` and then `=`, it would be a
 * syntax error somewhere past the point, if anywhere.
 */
int main(void) {
    int a = 0;
    int b = 1;
    a ||= b;
    return a;
}
