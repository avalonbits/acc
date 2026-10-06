/* ++ and -- through a pointer, as the machine-level backend makes them: a
 * byte stepped where it is -- inc (hl), dec (iy+d) -- with the value read
 * before it for x++ and after it for ++x; an int and a pointer read,
 * stepped and written back, through a member's displacement or a global's
 * address. Each answer has to be the old value or the new, as its type
 * has it: an unsigned char wrapping at 255, a signed one at 127, a pointer
 * stepping by what it points at, three bytes or thirteen. */
struct big { char pad[13]; };
struct rec { int n; signed char c; unsigned char u; int *p; struct big *b; };

unsigned char gu = 255;
signed char gs = 127;
int gi = 41;
int arr[3] = { 10, 20, 30 };
struct big bigs[2];
struct rec r = { 5, -128, 0, arr, bigs };

static int post_n(struct rec *x) { return x->n++; }
static int pre_n(struct rec *x) { return --x->n; }
static int post_c(struct rec *x) { return x->c--; }
static int pre_u(struct rec *x) { return ++x->u; }
static int take(struct rec *x) { return *x->p++; }
static int at(char *s) { (*s)++; return *s; }
static void bump(struct rec *x) { x->n++; x->u--; x->b++; }
static int post_gu(void) { return gu++; }
static int pre_gs(void) { return ++gs; }
static int post_gi(void) { return gi--; }

int main(void)
{
    int ok = 0;
    char s[2] = { 'a', 0 };

    if (post_n(&r) == 5 && r.n == 6) ok += 3;
    if (pre_n(&r) == 5 && r.n == 5) ok += 3;
    if (post_c(&r) == -128 && r.c == 127) ok += 3;
    if (pre_u(&r) == 1 && r.u == 1) ok += 3;
    if (take(&r) == 10 && take(&r) == 20 && r.p == arr + 2) ok += 3;
    if (at(s) == 'b' && s[0] == 'b') ok += 3;
    bump(&r);
    if (r.n == 6 && r.u == 0 && r.b == bigs + 1) ok += 3;
    if (post_gu() == 255 && gu == 0) ok += 3;
    if (pre_gs() == -128 && gs == -128) ok += 3;
    if (post_gi() == 41 && gi == 40) ok += 3;
    if (post_gu() == 0 && gu == 1) ok += 3;

    return ok + 9;
}
