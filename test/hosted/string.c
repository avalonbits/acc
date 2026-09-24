/* The rest of <string.h> -- strspn, strcspn, strpbrk, strtok, strcoll and
 * strxfrm -- and bsearch, against glibc's. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmp(const void *a, const void *b)
{
    int x = *(const int *) a, y = *(const int *) b;

    return x < y ? -1 : x > y;
}

int main(void)
{
    static const int sorted[] = { -5, 0, 3, 3, 8, 13, 21, 34, 55, 89 };
    char buf[64], x[8];
    char *tok;
    int i, k;

    printf("strspn %d %d %d\n", (int) strspn("aabbcx", "ab"), (int) strspn("", "ab"),
           (int) strspn("xyz", ""));
    printf("strcspn %d %d %d\n", (int) strcspn("hello, world", ",!"),
           (int) strcspn("abc", ""), (int) strcspn("", "x"));
    printf("strpbrk [%s] %d\n", strpbrk("path/to.file", "./"),
           strpbrk("abc", "xyz") == NULL);

    strcpy(buf, "  one,two;;three , four;");
    for (tok = strtok(buf, " ,;"); tok; tok = strtok(NULL, " ,;"))
        printf("strtok [%s]\n", tok);
    printf("strtok again %d\n", strtok(NULL, " ") == NULL);
    strcpy(buf, ";;;");
    printf("strtok empty %d\n", strtok(buf, ";") == NULL);
    strcpy(buf, "a b");
    printf("strtok [%s]", strtok(buf, " "));
    printf(" [%s]\n", strtok(NULL, ""));

    printf("strcoll %d %d %d\n", strcoll("a", "b") < 0, strcoll("b", "a") > 0,
           strcoll("same", "same"));
    printf("strxfrm %d", (int) strxfrm(x, "xform", sizeof x));
    printf(" [%s] %d\n", x, (int) strxfrm(NULL, "longer than none", 0));

    for (k = -6; k <= 97; k += 7) {
        const int *p = bsearch(&k, sorted, 10, sizeof *sorted, cmp);

        printf("bsearch %d: %d\n", k, p ? (int) (p - sorted) : -1);
    }
    for (i = 0; i < 10; i++) {
        const int *p = bsearch(&sorted[i], sorted, 10, sizeof *sorted, cmp);

        printf("%d", p && *p == sorted[i]);
    }
    printf(" %d\n", bsearch(&k, sorted, 0, sizeof *sorted, cmp) == NULL);

    /* Nothing past the end is looked at: the value after the array is the
     * one being looked for, and it must not be found. */
    {
        static struct { int a[10]; int after[4]; } guarded = {
            { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, { 11, 12, 13, 14 }
        };

        for (k = 11; k <= 14; k++)
            printf("past the end %d: %d\n", k,
                   bsearch(&k, guarded.a, 10, sizeof (int), cmp) == NULL);
    }

    return 0;
}
