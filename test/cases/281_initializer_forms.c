/* More forms gcc.dg found acc refusing, all C99:
 * - `int a[];` at file scope and never given a size has one element
 *   (6.9.2p5, decl-7);
 * - a scalar's initial value in braces, `int m = {0};` (6.7.8p11, pr112509);
 * - a char array's string in braces, `char s[] = {"foo"}` (6.7.8p14),
 *   wherever such an array is initialised (c99-complit-1);
 * - a subscript after va_arg (pr46130-1);
 * - a prototype naming a struct not yet complete (6.7.5.3p12, pr89211);
 * - a #pragma whose comment runs on to the next line (pragma-pack-3). */
#include <stdarg.h>

struct later;
int size_of(struct later);
struct later { char c[5]; };

int one[];
int m = { 7 };
char word[] = { "word" };
char *lit = (char []){ "lit" };
struct { char n[4]; int k; } rec = { { "abc" }, 3 };

static int pick(int n, ...)
{
    va_list ap;
    int c;

    va_start(ap, n);
    c = va_arg(ap, char *)[n];
    va_end(ap);

    return c;
}

int size_of(struct later l)
{
    return sizeof l.c;
}

int main(void)
{
    int r = 0, k = { 1, };
    char local[8] = { "loc", };
    struct later l;
#pragma pack(1) /* a comment that runs
                   on to the next line */

    one[0] = 5;
    if (one[0] == 5 && m == 7 && k == 1) r++;
    if (sizeof word == 5 && word[3] == 'd' && lit[2] == 't') r++;
    if (rec.n[2] == 'c' && rec.k == 3 && local[1] == 'o' && local[5] == 0) r++;
    if (pick(1, "xy") == 'y' && size_of(l) == 5) r++;

    return r + 38;              /* 4 checks */
}
