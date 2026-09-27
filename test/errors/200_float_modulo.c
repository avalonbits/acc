/* expect: 8:20: error: '%' takes integers, not floating-point values */
/* Not a missing routine: C requires integer operands for the remainder, so a
 * float here is the program's mistake. The same goes for the bitwise
 * operators and the shifts. */
int main(void) {
    float a = 5.0;
    float b = 2.0;
    float c = a % b;
    return 0;
}
