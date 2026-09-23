/* L as a name at file scope, beside the wide literals it begins.
 *
 * gcc's 20011219-1 has an enumeration constant called L, and va-arg-1 a
 * typedef. acc once took L for a keyword to find its wide literals, and a
 * keyword's code sits where a file-scope name's symbol is kept -- so every
 * one of those was "already declared". Now L is a name that the lexer
 * looks past only when a quote follows it. */
enum E { K = 40, L, M };

static int which(enum E e) {
    switch (e) {
    case L:
        return 1;
    default:
        return 0;
    }
}

int main(void) {
    int r = 0;

    if (which(L) && L == 41) r++;
    if (sizeof L"ab" == 3 * sizeof (short) && L"ab"[1] == 'b') r++;

    return r + 40;              /* 2 checks */
}
