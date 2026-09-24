/* A variable declared with no value, used, and then defined with one: C
 * makes the two one variable, with the value. acc gave the first room in
 * the bss, and what was compiled between the two was told to look there;
 * when the value came it put the bytes in the image and refused, since
 * nothing could point what was compiled at them. It now does. 20020611-1
 * is the torture test. */
unsigned int n;
int a[3];
struct S { int x, y; } s;

int *pa = &a[2];
int *end = a + 3;
int *py = &s.y;

static int total(void) { return n + a[1] + s.y; }
static int *where(void) { return (int *) &n; }

unsigned int n = 30;
int a[3] = { 1, 4, 9 };
struct S s = { 5, 8 };

int main(void)
{
    int r = 0;

    if (total() == 42) r++;
    if (*pa == 9 && end - a == 3 && *py == 8) r++;
    if (*where() == 30) r++;
    n = 1;
    if (total() == 13) r++;

    return r + 38;              /* 4 checks */
}
