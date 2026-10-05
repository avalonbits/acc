/* Addresses as constants, as opt-acc's machine-level backend loads them:
 * a string's, moved where the function's code put it -- made smaller than
 * the first pass's, so the string is somewhere else -- and a static's in
 * the bss; a branch on one, which is never 0; and a bit-field of a global
 * struct, read through the global's address -- which the backend does not
 * make, and must leave to the others, as gcc's field-merge-20 found. */
typedef struct {
    int p : 8;
    int d : 1;
    int b : 6;
    int e : 1;
} bits;

bits a = { .d = 1, .e = 1 }, c = { .b = 1, .d = 1, .e = 1 };
static int counter;
int seen;

const char *pick(int k, int m)
{
    int t = k * 3 + m;

    if (t > 4 && t != m * 7)
        return "yes";
    return "no";
}

int *count_of(void)
{
    return &counter;
}

int always(void)
{
    if (&seen)
        return 5;
    return 6;
}

int same_bits(void)
{
    if (a.d == c.d && a.e == c.e)
        return 0;
    return -1;
}

int main(void)
{
    int r = 0;

    r += pick(2, 2)[0] == 'y' && pick(0, 1)[1] == 'o';
    *count_of() = 7;
    r += counter == 7 && count_of() == &counter;
    r += always() == 5;
    r += same_bits() == 0;

    return r == 4 ? 42 : r;
}
