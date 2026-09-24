/* Forms gcc.dg found acc refusing, all C99:
 * - `sizeof (0, x.c)`: the comma's right side, a value, so an array there
 *   is a pointer; `sizeof (c = 1000)`: c's type, and c left alone;
 * - `i[a]`, which is `*(i + a)` as much as `a[i]` is (6.5.2.1);
 * - `j ? p : n` with n a `void *`, which meet at `void *` (6.5.15p6);
 * - a name an if or a while declares in its condition ending with it, so
 *   that one it hid is seen again (6.8.4p3, 6.8.5p5);
 * - sizeof of a VLA operand, which is evaluated (6.5.3.4p2). */
typedef int T;

struct s { char c[17]; } x;
static void *const nothing = 0;

int main(void)
{
    int r = 0, i = 1, a[3] = { 5, 42, 7 }, *p = a, j = 1;
    char c = 0;
    void *v;

    if (sizeof (x.c) == 17 && sizeof (0, x.c) == sizeof (char *)) r++;
    if (sizeof (c = 1000) == 1 && sizeof (c += 1, 0L) == 4 && c == 0) r++;
    if (i[a] == 42 && (i + 1)[p] == 7 && i["ab"] == 'b') r++;
    v = j ? p : nothing;
    if (v == p) r++;
    if (sizeof (enum { T }) == 0)
        ;
    {
        T t = 3;                /* the typedef again */

        while (sizeof (enum { T = 5 }) == 0)
            ;
        if (t == 3 && sizeof (T) == sizeof (int)) r++;
    }
    {
        int n = 1;

        if (sizeof (*(++n, (char (*)[n]) 0)) == 2 && n == 2) r++;
    }

    return r + 36;              /* 6 checks */
}
