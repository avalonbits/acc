/* A parenthesis round an object leaves an object (C99 6.5.1p5), so it can
 * be assigned to, stepped and subscripted: `(x)++`, `(p->n) += 2`. Macros
 * put one round nearly everything -- `#define COUNT (count)` and then
 * `COUNT++`. acc read what a parenthesis held to a value, and refused the
 * `++` and the `=` after it. */
#define COUNT (count)

struct node { int n; char *pos; };

int count;
int g[3];

static int next(struct node *d)
{
    return *(d->pos)++;         /* 20060910-1 */
}

int main(void)
{
    int r = 0, x = 5, y, c, a[2] = { 7, 7 };
    char s[] = "ab";
    struct node node = { 1, s }, *p = &node;

    (x)++;
    if (x == 6) r++;
    (x) = 10;
    (x) += 5;
    if (x == 15) r++;
    COUNT++;
    COUNT += 2;
    if (count == 3) r++;
    (p->n) += 2;
    (node.n)++;
    if (node.n == 4) r++;
    (a[1])--;
    (g[2]) = 9;
    if (a[1] == 6 && g[2] == 9) r++;
    if (next(p) == 'a' && next(p) == 'b') r++;
    y = (x)++ + 1;              /* the old value, then the step */
    if (y == 16 && x == 16) r++;

    /* And a parenthesis that is only a value binds as it always did. */
    if (2 * (x) + 1 == 33 && (x + 1) * 2 == 34) r++;
    if ((c = x) != 0 && c == 16) r++;
    if ((p)->n == 4 && (s)[1] == 'b' && (node).n == 4) r++;

    return r + 32;              /* 10 checks */
}
