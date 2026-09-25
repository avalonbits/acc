/* What a pointer points at, in parentheses, changed inside an expression:
 * `x = ((*p) &= 3)`, `((**q) = 2)`, `((*p)++)`, and `--(*p)`, `++(n)` --
 * a prefix step's operand in parentheses. Csmith writes all of these. */
static int twice(int v)
{
    return v * 2;
}

int main(void)
{
    int n = 7, m = 1, *p = &n, **q = &p;
    int a, b, c, d, e;

    a = ((*p) &= 3);                    /* n = 3, a = 3 */
    b = ((**q) = 2) + 1;                /* n = 2, b = 3 */
    c = twice(((*p)++));                /* c = 4, n = 3 */
    d = --(*p) + --((*p));              /* n = 2, then 1: d = 3 */
    e = ++(m) + (++(m));                /* m = 2, then 3: e = 5 */

    return a + b + c + d + e + n + m * 8 - 1;   /* 43 - 1 */
}
