/* The address of a string literal, and of a byte inside one.
 *
 * A string literal is an array with static storage, so it is an object and
 * `&` works on it the way it works on a named array: the address of the
 * whole is its first byte's, and the address of an element is that element.
 * acc read the operand of `&` as a small grammar of its own -- a name, a
 * star, or a bracket -- and a string was none of the three, so it said the
 * address of one could not be taken.
 *
 * What it still cannot do is fold one into a global's initial value:
 * `char *p = &"X"[0];` is a constant address and acc asks for a constant it
 * can see. That is a different path and a separate gap.
 */
static int len(const char *s) { int n = 0; while (s[n]) n++; return n; }

int main(void) {
    int r = 0;
    const char *inside = &"hello"[1];
    const char (*whole)[3] = &"ab";
    const char *first = &"world"[0];

    if (*inside == 'e' && len(inside) == 4) r++;
    if ((*whole)[0] == 'a' && (*whole)[1] == 'b' && (*whole)[2] == 0) r++;

    /* The address of the first byte is the string's own, which is what
     * makes `&s[0]` and `s` the same thing. Compared against a second
     * `"world"` it would not be: whether two literals spelled the same are
     * one object is left to the implementation, and acc writes each where
     * it is read. */
    if (*first == 'w' && len(first) == 5) r++;
    /* The type of the whole is an array of every byte and the null after
     * it, measured where nothing else has declared what it is. */
    if (sizeof *whole == 3 && sizeof *&"abcd" == 5) r++;

    /* One inside a longer expression, where the subscript is worked out
     * rather than written down. */
    {
        int i = len("ab");

        if (*&"abcd"[i] == 'c') r++;
    }

    /* And through a pointer to the whole, which steps over all of it. */
    if ((int) (sizeof "ab") == 3 && whole + 1 != whole) r++;

    /* A parenthesis round the string says only where the operand ends, so
     * the subscript after it binds to what was inside: `&("abcd")[1]` is
     * the address of the 'b', not of anything past the array. */
    {
        int i = len("abc");

        if (*&("abcd")[1] == 'b' && *&("abcd")[i] == 'd') r++;
    }

    return r + 35;              /* 7 checks */
}
