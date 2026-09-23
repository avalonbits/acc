/* Wide character constants and wide strings, C99 6.4.4.4 and 6.4.5.
 *
 * wchar_t is agondev's, a signed short, so a wide string is UTF-16: a
 * character past 0xffff is the two halves of a surrogate pair. A narrow
 * string joined to a wide one is read as though it were wide, its UTF-8
 * taken apart into characters. And L is still a name, and a macro's, when
 * no quote follows it. */
typedef short wchar_t;

static wchar_t global[] = L"xyz";
static wchar_t padded[5] = L"ab";
static struct { int n; wchar_t s[4]; } rec = { 1, L"hi" };

#define L 5
static int from_macro = L;
#undef L

/* In an #if too, where L and the quote are one token as well. */
#if L'a' == 97 && L'\xffff' < 0 && 'a' == 97
static int in_if = 1;
#else
static int in_if = 0;
#endif

int main(void) {
    int r = 0, L = 3;
    wchar_t local[] = L"\u00e9t\u00e9";
    const wchar_t *joined = "ab" L"c";
    const wchar_t *mixed = L"a" "\u00e9";
    const wchar_t *face = L"\U0001F600";

    if (sizeof (L'a') == 2 && L'a' == 97 && L'\xffff' == -1
        && L'\u8000' == -32768 && L'\377' == 255) r++;
    if (sizeof (L"ab") == 6 && L"ab"[1] == 'b' && L"ab"[2] == 0) r++;
    if (sizeof (L"\U0001F600") == 6 && face[0] == (wchar_t) 0xd83d
        && face[1] == (wchar_t) 0xde00 && face[2] == 0) r++;
    if (sizeof ("ab" L"c") == 8 && joined[2] == 'c' && joined[3] == 0) r++;
    if (mixed[0] == 'a' && mixed[1] == 0xe9 && mixed[2] == 0) r++;
    if (sizeof global == 8 && global[2] == 'z' && global[3] == 0) r++;
    if (sizeof padded == 10 && padded[1] == 'b' && padded[4] == 0) r++;
    if (sizeof local == 8 && local[0] == 0xe9 && local[1] == 't') r++;
    if (rec.s[1] == 'i' && rec.s[2] == 0 && rec.n == 1) r++;
    if (sizeof (*&L"ab") == 6) r++;
    if (L + from_macro == 8 && in_if) r++;

    return r + 31;              /* 11 checks */
}
