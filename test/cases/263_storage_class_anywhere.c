/* A storage class among a declaration's specifiers rather than first:
 * `const static int`, `int static`, `struct p extern`, `int typedef`. C99
 * 6.11.5 calls that obsolescent, which is not the same as not C, and acc
 * refused it -- two torture tests say `const static int` in a block. */
const static int t[] = { 30 };
int static s = 1;
int typedef I;
struct p { int a; } extern ep;
struct p ep = { 2 };

static int count(void)
{
    int static calls;           /* kept from one call to the next */

    return ++calls;
}

int main(void)
{
    const static int u[] = { 3 };
    long register w = 1;
    I typedef J;
    J x = 0;
    int i;

    for (int register k = 0; k < 2; k++)
        x += k;
    for (i = 0; i < 4; i++)
        count();

    /* 30 + 1 + 3 + 1 + 2 + 1 + 4 */
    return t[0] + s + u[0] + (int) w + ep.a + x + count() - 1;
}
