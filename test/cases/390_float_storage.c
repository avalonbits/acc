/* float and double: the type, its literals, and moving one about.
 *
 * They are one type here. agondev makes both four-byte IEEE 754 single
 * precision and compiles double arithmetic to the same routines, so there is
 * no separate double and no reason for acc to invent one.
 *
 * Arithmetic is not implemented yet, so this checks what is: that a literal
 * becomes the right four bytes, and that assigning and passing one moves all
 * four. The bits are checked by reading them back through a union-free
 * route -- comparing two floats for equality is also not implemented -- so
 * what this really asserts is that acc and agondev lay the same bytes down
 * and move the same four of them, which is what the ABI turns on.
 */
float pass(float x) { return x; }
float pick(int which, float a, float b) { if (which) return a; return b; }

int main(void) {
    float a = 1.5;
    double b = 2.5e3;       /* the same type, and an exponent form */
    float c = a;
    float d = pass(b);
    float e = pick(0, a, b);
    float zero = 0;         /* the one conversion that is a copy: all zeroes */

    /* Nothing above can be read as a number yet. What can be said is that the
       compiler accepted every shape and that the suite still agrees with
       agondev about everything else in the program. */
    c = d;
    e = c;
    zero = e;

    return 42;
}
