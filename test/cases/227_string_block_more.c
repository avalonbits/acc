/* The rest of <string.h> that the C library has in assembly -- strncpy,
 * strncat, strstr, strspn, strcspn, strpbrk, strchrnul, strncasecmp, and
 * memcpy, memmove, memset and memchr -- at their edges: counts of nothing,
 * of one, and past the string; overlap both ways; sets that are empty; a
 * needle at the start, at the end, and not there.
 *
 * The mem functions are called through pointers: a call by name is to acc's
 * runtime's own, and this is about the library's.
 */
typedef unsigned int size_t;
char *strncpy(char *, const char *, size_t);
char *strncat(char *, const char *, size_t);
char *strstr(const char *, const char *);
size_t strspn(const char *, const char *);
size_t strcspn(const char *, const char *);
char *strpbrk(const char *, const char *);
char *strchrnul(const char *, int);
int strncasecmp(const char *, const char *, size_t);
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
void *memchr(const void *, int, size_t);
int strcmp(const char *, const char *);

static int sign(int v) { return v < 0 ? -1 : v > 0; }

/* A string the compiler cannot see into: agondev's clang, folding these
 * calls on constant strings, loops in its optimiser and gives up. */
static const char *op(const char *p)
{
    const char *volatile q = p;

    return q;
}

static int same(const char *a, const char *b, int n)
{
    int i;

    for (i = 0; i < n; i++)
        if (a[i] != b[i])
            return 0;

    return 1;
}

int main(void) {
    void *(*cpy)(void *, const void *, size_t) = memcpy;
    void *(*move)(void *, const void *, size_t) = memmove;
    void *(*set)(void *, int, size_t) = memset;
    void *(*chr)(const void *, int, size_t) = memchr;
    /* strstr too, through a pointer clang cannot see: on its own name
     * agondev's clang folds the calls here and loops doing it. */
    char *(*volatile sstr)(const char *, const char *) = strstr;
    char b[16], m[16];
    const char *s = op("abcabc");
    int r = 0;

    set(b, 'x', 16);
    if (strncpy(b, op("abc"), 6) == b && same(b, "abc\0\0\0x", 7)) r++;
    set(b, 'x', 16);
    if (strncpy(b, op("abcdef"), 3) == b && same(b, "abcx", 4)) r++;
    set(b, 'x', 16);
    if (strncpy(b, op(""), 1) == b && same(b, "\0x", 2) && strncpy(b, op("q"), 0) == b
        && b[0] == 0) r++;

    strncpy(b, op("ab"), 16);
    if (strncat(b, op("cdef"), 2) == b && strcmp(b, op("abcd")) == 0) r++;
    if (strncat(b, op("xy"), 9) == b && strcmp(b, op("abcdxy")) == 0
        && strncat(b, op("zz"), 0) == b && strcmp(b, op("abcdxy")) == 0) r++;

    if (sstr(s, op("")) == s && sstr(s, op("abc")) == s && sstr(s, op("cab")) == s + 2)
        r++;
    if (sstr(s, op("bcx")) == 0 && sstr(s, op("abcabcd")) == 0
        && sstr(s, op("c")) == s + 2 && sstr(op(""), op("a")) == 0) r++;

    if (strspn(op("aabxc"), op("ab")) == 3 && strspn(op("xab"), op("ab")) == 0
        && strspn(op("abab"), op("ab")) == 4 && strspn(op("abc"), op("")) == 0) r++;
    if (strcspn(op("xyab"), op("ab")) == 2 && strcspn(op("abc"), op("")) == 3
        && strcspn(op(""), op("ab")) == 0 && strcspn(op("xyz"), op("ab")) == 3) r++;
    if (strpbrk(s, op("cb")) == s + 1 && strpbrk(s, op("xyz")) == 0
        && strpbrk(s, op("")) == 0) r++;

    if (strchrnul(s, 'c') == s + 2 && strchrnul(s, 'z') == s + 6
        && strchrnul(s, 0) == s + 6) r++;

    if (strncasecmp(op("Hello"), op("hELLO"), 5) == 0 && strncasecmp(op("abc"), op("abd"), 2) == 0
        && strncasecmp(op("x"), op("y"), 0) == 0) r++;
    if (sign(strncasecmp(op("abc"), op("ABD"), 3)) == -1 && sign(strncasecmp(op("b"), op("A"), 1)) == 1
        && sign(strncasecmp(op("ab"), op("AbC"), 9)) == -1 && strncasecmp(op("["), op("{"), 1) != 0)
        r++;

    set(m, 0, 16);
    if (cpy(m, op("hello"), 6) == m && strcmp(m, op("hello")) == 0 && cpy(m, op("zz"), 0) == m
        && m[0] == 'h') r++;
    cpy(m, op("abcdefgh"), 9);
    if (move(m + 2, m, 4) == m + 2 && same(m, "ababcdgh", 8)) r++;
    cpy(m, op("abcdefgh"), 9);
    if (move(m, m + 2, 4) == m && same(m, "cdefefgh", 8)
        && move(m, m, 3) == m && same(m, "cdef", 4)) r++;
    if (set(m, 'q', 1) == m && m[0] == 'q' && m[1] == 'd' && set(m, 'z', 0) == m
        && m[0] == 'q') r++;
    if (chr(s, 'c', 6) == s + 2 && chr(s, 'c', 2) == 0 && chr(s, 'a', 0) == 0
        && chr(s, 0, 7) == s + 6) r++;

    return r + 24;              /* 18 checks */
}
