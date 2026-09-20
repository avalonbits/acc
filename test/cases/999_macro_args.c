/* Macros with parameters, against agondev compiling the same program.
 *
 * What another compiler is worth asking here is the shape of the
 * expansion: that an argument goes in where the parameter was, that it is
 * expanded first unless `#` or `##` has it, that a comma inside
 * parentheses stays inside them, and that `...` collects the rest. Each of
 * those leaves different code behind if it is got wrong.
 */
#define TWICE(x)        ((x) * 2)
#define QUAD(x)         TWICE(TWICE(x))
#define PICK_SECOND(a, b) (b)
#define NAME_OF(x)      #x
#define JOIN(a, b)      a ## b
#define FIRST_OF(a, ...) (a)
#define BASE            5

int JOIN(part, one)(int v) { return v + 1; }

int main(void) {
    char *text = NAME_OF(BASE);     /* "BASE", not "5" */
    int total = 0;

    total = total + QUAD(BASE);             /* 20 */
    total = total + PICK_SECOND(f(1, 2), 9);/* 9   */
    total = total + FIRST_OF(11, 1, 2);     /* 11  */
    total = total + partone(1);             /* 2   */

    /* The string is the name as it was written, so its first byte is 'B'
     * and not '5'. */
    return text[0] == 'B' ? total : 0;
}
