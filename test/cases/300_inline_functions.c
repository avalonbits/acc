/* Calls to static inline functions whose body is one return, which acc
 * compiles in place: they have to come to what a call would. Arguments
 * with side effects are evaluated once, before the body, and converted to
 * the parameter's type; the answer is converted to the function's; a name
 * the body uses means what it meant where the body was written, even where
 * the caller has one of its own by that name, or a macro has been defined
 * again since; a body that calls itself stops; and the function is still
 * there to take the address of. */
static const unsigned char classes[256] = { [' '] = 1, ['\t'] = 1, ['a'] = 2,
                                            ['z'] = 2, [0x80] = 3 };
#define SPACE 1

static inline _Bool is_space(char c) { return (classes[(unsigned char) c] & SPACE) != 0; }
static inline unsigned char low(int x) { return x; }
static inline signed char slow(int x) { return x; }
static inline int twice(int x) { return x + x; }
static inline int bump(int x) { return (x += 3) * 2; }
static inline int quad(int x) { return twice(twice(x)); }
static inline int fact(int n) { return n > 1 ? n * fact(n - 1) : 1; }
static inline const char *skip(const char *p) { return *p == ' ' ? p + 1 : p; }
static inline int pick(int a, short b, unsigned char c) { return a - b * c; }
static int shadowed = 7;
static inline int uses_global(int x) { return x + shadowed; }

#undef SPACE
#define SPACE 2

static unsigned long mix(unsigned long h, long x)
{
    return (h * 31 + (unsigned long) x) & 0xffffffffUL;
}

static int count;
static int next_i(void) { return ++count; }

int main(void)
{
    static const char text[] = " a\tz";
    unsigned long h = 0;
    int (*fp)(int) = twice;
    const char *p = text;
    int i = 0, n;

    for (n = 0; n < 5; n++) {
        h = mix(h, is_space(text[n]));
        if (is_space(text[n]) && n) h = mix(h, 1);
        h = mix(h, is_space(text[n]) ? 2 : 3);
    }
    h = mix(h, low(0x1ff));
    h = mix(h, slow(0x1ff));
    h = mix(h, low(-1) + slow(200));
    h = mix(h, twice(i++));
    h = mix(h, i);
    h = mix(h, twice(next_i()));
    h = mix(h, count);
    h = mix(h, bump(4));
    h = mix(h, quad(3) + quad(-2));
    h = mix(h, fact(6));
    h = mix(h, fp(21));
    h = mix(h, (int) (skip(skip(p)) - text));
    h = mix(h, pick(1000, -3, 300));
    h = mix(h, pick(twice(2), low(0x305), 2) * twice(pick(1, 2, 3)));
    h = mix(h, uses_global(1));
    {
        int shadowed = 100;

        h = mix(h, uses_global(1) + shadowed);
    }
    h = mix(h, sizeof twice(i++) == sizeof (int));
    h = mix(h, i);
    h = mix(h, twice(1) + twice(2) + twice(3) + twice(4) + twice(5) + twice(6));

    /* From gcc on the host: every value fits the machine's int. */
    return h == 2048142568UL ? 42 : 1;
}
