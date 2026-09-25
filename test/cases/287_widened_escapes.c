/* A narrow string joined to a wide one is read as wide (C99 6.4.5). An
 * escape in it is one wide character, as it would be in L"...", where the
 * UTF-8 of a character written in the source is taken apart into the one
 * character it spells: "\343\201\202" L"" is three, "あ" L"" is one. From
 * chibicc's string.c. A wide character is a short, sixteen bits, to both
 * compilers. */
typedef short wchar_t;

int main(void)
{
    const wchar_t *three = "\343\201\202" L"";
    const wchar_t *one = "あ" L"";
    const wchar_t *after = L"" "\343x";
    int n = 0;

    n += three[0] == 0343 && three[1] == 0201 && three[2] == 0202;
    n += three[3] == 0;
    n += one[0] == 0x3042 && one[1] == 0;
    n += after[0] == 0343 && after[1] == 'x';
    n += sizeof("\343\201\202" L"") == 4 * sizeof(wchar_t);

    return n == 5 ? 42 : n;
}
