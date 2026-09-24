/* What a macro puts in a parameter's place is tokens, and so is its body:
 * where the two meet they stay separate even when their spellings would run
 * together. acc builds an expansion as text, and `#define NEG(x) -x` with
 * -1 made `--1`, a decrement; c-testsuite's 00202 pastes a `+` to nothing
 * right before the body's own `+`, and got `++` where there are two. A
 * paste, `##`, joins on purpose, and still does. */
#define NEG(x) -x
#define Q(A, B) A ## B+
#define CAT(x, y) x y
#define ID(x) x
#define GLUE(a, b) a ## b

int main(void)
{
    int r = 0, a = 1, j, GLUE(val, ue) = 7;

    if (NEG(-1) == 1) r++;
    j = 60 Q(+,)3;
    if (j == 63) r++;
    if ((a CAT(+, +) 1) == 2) r++;                 /* `a + + 1` */
    if ((a ID(-)-1) == 2) r++;
    if (GLUE(val, ue) == 7 && GLUE(4, 2) == 42) r++;

    return r + 37;              /* 5 checks */
}
