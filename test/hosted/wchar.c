/* <wchar.h> against glibc in its UTF-8 locale: the wide strings, the
 * conversions to and from UTF-8 with what they refuse, the numbers, wide
 * characters on a file, swprintf and swscanf, and <stdlib.h>'s multibyte
 * functions. Wide strings are shown as their characters' values in hex,
 * which reads the same whatever the width of the host's wchar_t. */
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void show(const char *what, const wchar_t *s)
{
    printf("%s:", what);
    if (!s) {
        printf(" null\n");
        return;
    }
    for (; *s; s++)
        printf(" %x", (unsigned) *s & 0xffff);
    printf("\n");
}

static void bytes(const char *what, const char *s, size_t n)
{
    size_t i;

    printf("%s %d:", what, (int) n);
    for (i = 0; i < n; i++)
        printf(" %02x", (unsigned char) s[i]);
    printf("\n");
}

/* Each input, a byte at a time through mbrtowc, then all at once. */
static void decode(const char *s, size_t n)
{
    mbstate_t st;
    wchar_t wc;
    size_t i, r;

    memset(&st, 0, sizeof st);
    printf("decode");
    for (i = 0; i < n; i++) {
        errno = 0;
        wc = 0;
        r = mbrtowc(&wc, s + i, 1, &st);
        if (r == (size_t) -2)
            printf(" .");
        else if (r == (size_t) -1) {
            printf(" bad%s", errno == EILSEQ ? "" : "?");
            memset(&st, 0, sizeof st);
        } else
            printf(" %x/%d", (unsigned) wc & 0xffff, (int) r);
    }
    memset(&st, 0, sizeof st);
    r = mbrtowc(&wc, s, n, &st);
    printf(" | whole %d", r == (size_t) -1 ? -1 : r == (size_t) -2 ? -2 : (int) r);
    printf(" mblen %d\n", mblen(s, n));
}

int main(void)
{
    wchar_t w[64], w2[64], *end, *save, *tok;
    char b[64];
    const char *src;
    const wchar_t *wsrc;
    mbstate_t st;
    size_t r;
    int i;

    setlocale(LC_CTYPE, "C.UTF-8");
    printf("MB_CUR_MAX %d, MB_LEN_MAX %d\n", (int) MB_CUR_MAX >= 3, MB_LEN_MAX >= 3);

    /* Strings. */
    wcscpy(w, L"café au lait");
    show("wcscpy", w);
    printf("wcslen %d\n", (int) wcslen(w));
    wcscat(w, L" €");
    show("wcscat", w);
    wcsncat(w, L"xyz", 2);
    show("wcsncat", w);
    wcsncpy(w2, L"ab", 5);
    printf("wcsncpy %x %x %x\n", (unsigned) w2[1], (unsigned) w2[2], (unsigned) w2[4]);
    printf("wcscmp %d %d %d\n", wcscmp(L"abc", L"abd") < 0, wcscmp(L"abd", L"abc") > 0,
           wcscmp(L"é", L"é"));
    printf("wcsncmp %d %d\n", wcsncmp(L"abcx", L"abcy", 3), wcsncmp(L"ab", L"ac", 5) < 0);
    printf("wcscoll %d\n", wcscoll(L"a", L"b") < 0);
    show("wcschr", wcschr(w, L'é'));
    show("wcsrchr", wcsrchr(w, L'a'));
    show("wcschr none", wcschr(w, L'Q'));
    printf("wcsspn %d wcscspn %d\n", (int) wcsspn(w, L"acf"), (int) wcscspn(w, L" "));
    show("wcspbrk", wcspbrk(w, L"u€"));
    show("wcsstr", wcsstr(w, L"au"));
    show("wcsstr empty", wcsstr(w, L""));
    show("wcsstr none", wcsstr(w, L"tea"));
    wcscpy(w2, L"  one, two,,three ");
    for (tok = wcstok(w2, L" ,", &save); tok; tok = wcstok(NULL, L" ,", &save))
        show("wcstok", tok);
    wmemset(w2, L'z', 3);
    wmemcpy(w2 + 3, L"abc", 4);
    show("wmemset/wmemcpy", w2);
    wmemmove(w2 + 1, w2, 5);
    show("wmemmove", w2);
    printf("wmemcmp %d wmemchr %d\n", wmemcmp(L"ab", L"ac", 2) < 0,
           (int) (wmemchr(w2, L'b', 6) - w2));
    printf("wcsxfrm %d\n", (int) wcsxfrm(w2, L"xfrm", 10));

    /* Multibyte, one character at a time. */
    decode("A", 1);
    decode("\xc3\xa9", 2);
    decode("\xe2\x82\xac", 3);
    decode("\xef\xbf\xbd", 3);
    decode("\x80", 1);
    decode("\xc0\x80", 2);
    decode("\xe0\x80\x80", 3);
    decode("\xed\xa0\x80", 3);
    decode("\xc3", 1);
    decode("\xc3\x41", 2);
    decode("\xfe", 1);
    decode("", 1);
    memset(&st, 0, sizeof st);
    printf("mbsinit %d", mbsinit(&st));
    mbrtowc(NULL, "\xe2", 1, &st);
    printf(" %d", mbsinit(&st));
    printf(" mbrlen %d\n", (int) mbrlen("\x82\xac", 2, &st));
    printf("btowc %x %d wctob %d %d\n", (unsigned) btowc('A'), btowc(0xe9) == WEOF,
           wctob(L'A'), wctob(L'é'));

    for (i = 0; i < 6; i++) {
        static const wchar_t cs[] = { L'A', 0xe9, 0x7ff, 0x800, 0x20ac, 0xd800 };

        memset(&st, 0, sizeof st);
        errno = 0;
        r = wcrtomb(b, cs[i], &st);
        if (r == (size_t) -1)
            printf("wcrtomb %x bad %d\n", (unsigned) cs[i] & 0xffff, errno == EILSEQ);
        else
            bytes("wcrtomb", b, r);
    }
    printf("wcrtomb null %d\n", (int) wcrtomb(NULL, L'x', NULL));

    /* Whole strings. */
    src = "h\xc3\xa9llo \xe2\x82\xac!";
    memset(&st, 0, sizeof st);
    r = mbsrtowcs(w, &src, 64, &st);
    printf("mbsrtowcs %d %d", (int) r, src == NULL);
    show("", w);
    src = "h\xc3\xa9llo";
    r = mbsrtowcs(w, &src, 2, &st);
    printf("mbsrtowcs 2: %d left %d\n", (int) r, (int) strlen(src));
    src = "ab\xffz";
    errno = 0;
    r = mbsrtowcs(w, &src, 64, &st);
    printf("mbsrtowcs bad %d %d at %d\n", r == (size_t) -1, errno == EILSEQ, (int) strlen(src));
    src = "h\xc3\xa9";
    printf("mbsrtowcs count %d\n", (int) mbsrtowcs(NULL, &src, 0, &st));
    wsrc = L"été €";
    memset(b, 'Z', sizeof b);
    r = wcsrtombs(b, &wsrc, 64, &st);
    bytes("wcsrtombs", b, r + 1);
    wsrc = L"été";
    r = wcsrtombs(b, &wsrc, 4, &st);
    printf("wcsrtombs 4: %d left %d\n", (int) r, (int) wcslen(wsrc));
    wsrc = L"€€";
    printf("wcsrtombs count %d\n", (int) wcsrtombs(NULL, &wsrc, 0, &st));
    printf("mbstowcs %d wcstombs %d\n", (int) mbstowcs(w, "\xc3\xa9t\xc3\xa9", 64),
           (int) wcstombs(b, L"été", 64));
    printf("mbtowc %d %d wctomb %d %d\n", mbtowc(w, "\xe2\x82\xac", 3), mbtowc(NULL, 0, 0),
           wctomb(b, L'é'), wctomb(NULL, 0));

    /* Numbers. */
    errno = 0;
    printf("wcstol %ld", wcstol(L"  -123abc", &end, 10));
    show(" rest", end);
    printf("wcstoul %lu wcstoll %lld wcstoull %llu\n", wcstoul(L"0x1F", NULL, 0),
           wcstoll(L"-9000000000", NULL, 10), wcstoull(L"777", NULL, 8));
    printf("wcstol over %d %d\n", wcstol(L"99999999999999999999", NULL, 10) == LONG_MAX,
           errno == ERANGE);
    printf("wcstod %a", wcstod(L" 1.5e3 x", &end));
    show(" rest", end);
    printf("wcstof %a %a\n", wcstof(L"0x1.8p1", NULL), wcstof(L"-inf", NULL));

    /* swprintf and swscanf. */
    {
        int n, a = 0;
        float f = 0;
        char nb[16];
        wchar_t ws[16];

        n = swprintf(w, 64, L"[%d|%5.2f|%-4ls|%6s|%lc|%c|%x|%3ls|%.2s]%n", 42, 3.14159,
                     L"é", "\xc3\xa9t\xc3\xa9", L'€', 'q', 255, L"ab", "\xc3\xa9t\xc3\xa9",
                     &a);
        printf("swprintf %d %d", n, a);
        show("", w);
        printf("swprintf short %d", swprintf(w, 5, L"%s", "abcdefgh") < 0);
        show("", w);
        n = swscanf(L" 17 2.5 wörd €x", L"%d %f %ls %hs", &a, &f, ws, nb);
        printf("swscanf %d %d %a", n, a, f);
        show("", ws);
        bytes(" narrow", nb, strlen(nb));
        printf("swscanf eof %d\n", swscanf(L"", L"%d", &a));
    }

    /* A file, written wide and read back both ways. */
    {
        FILE *f = fopen("t_wide.txt", "w");
        wint_t c;

        printf("fwide %d", fwide(f, 0));
        fputws(L"été\n", f);
        fputwc(L'€', f);
        fputwc(L'!', f);
        printf(" then %d\n", fwide(f, 0) > 0);
        fclose(f);
        f = fopen("t_wide.txt", "r");
        i = 0;
        while ((c = fgetc(f)) != EOF)
            b[i++] = (char) c;
        bytes("bytes", b, (size_t) i);
        printf("byte stream fwide %d\n", fwide(f, 0) < 0);
        fclose(f);
        f = fopen("t_wide.txt", "r");
        show("fgetws", fgetws(w, 64, f));
        c = fgetwc(f);
        printf("fgetwc %x ungetwc %x again %x", (unsigned) c, (unsigned) ungetwc(L'?', f),
               (unsigned) fgetwc(f));
        printf(" then %x then %d\n", (unsigned) fgetwc(f), fgetwc(f) == WEOF);
        fclose(f);
        f = fopen("t_wide.txt", "w");
        fputs("ok\xff", f);
        fclose(f);
        f = fopen("t_wide.txt", "r");
        errno = 0;
        printf("fgetwc %x %x", (unsigned) fgetwc(f), (unsigned) fgetwc(f));
        printf(" bad %d %d\n", fgetwc(f) == WEOF, errno == EILSEQ);
        fclose(f);
        remove("t_wide.txt");
    }

    return 0;
}
