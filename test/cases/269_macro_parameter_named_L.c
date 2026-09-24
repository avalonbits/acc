/* `L'1'` is one token, a wide character constant, and its L is not a name.
 * A macro whose parameter is called L put the argument in the L's place and
 * made `0'1'`, which is two tokens and not C. 970214-2 is the torture
 * test; an object-like macro called L must leave the literals alone too. */
#define m(L) (L'1' + (L))
#define w(L) (L"ab"[L])
#define L 7

int main(void)
{
    int r = 0;

    if (m(0) == L'1' && m(2) == L'1' + 2) r++;
    if (w(1) == 'b') r++;
    if (L == 7 && L'c' == 'c') r++;

    return r + 39;              /* 3 checks */
}
