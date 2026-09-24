/* The wide printf family on stdout, which a stream only allows once it is
 * wide -- so a program of its own, with nothing narrow written to stdout.
 * What it prints is ASCII, so that the emulator's screen shows it as the
 * host's terminal does. And wscanf, from stdin made a file. */
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

static int vw(const wchar_t *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vwprintf(fmt, ap);
    va_end(ap);

    return n;
}

int main(void)
{
    int n, a = 0, b = 0;
    long l;
    wchar_t w[16];
    FILE *f;

    setlocale(LC_CTYPE, "C.UTF-8");
    n = wprintf(L"%d %5d|%-5d|%05d %+d % d\n", 42, -7, 7, 42, 3, 4);
    wprintf(L"count %d\n", n);
    wprintf(L"%x %X %#o %#x %lu %lld %hhd %hd\n", 255u, 255u, 8u, 255u, 4000000000UL,
            -9000000000000LL, 300, 70000);
    wprintf(L"%.3f %e %g %a %10.2f|\n", 3.14159, 1234.5, 0.0001, 1.5, -2.25);
    wprintf(L"[%s] [%10s] [%-6s] [%.2s] [%ls] [%*ls] [%.*ls]\n", "abc", "right", "left",
            "trunc", L"wide", 6, L"pad", 2, L"prec");
    wprintf(L"[%c%c] [%lc] [%3c] [%-3c] %%\n", 'o', 'k', L'W', 'r', 'l');
    wprintf(L"n:%n|\n", &a);
    wprintf(L"a %d\n", a);
    vw(L"vwprintf %s %d\n", "ok", 1);
    fwprintf(stdout, L"fwprintf %ls\n", L"too");
    putwchar(L'p');
    putwc(L'w', stdout);
    fputwc(L'\n', stdout);
    fputws(L"fputws\n", stdout);

    f = fopen("t_win.txt", "w");
    fputs("12 34 word 5", f);
    fclose(f);
    freopen("t_win.txt", "r", stdin);
    n = wscanf(L"%d %d %ls %ld", &a, &b, w, &l);
    wprintf(L"wscanf %d: %d %d [%ls] %ld\n", n, a, b, w, l);
    f = fopen("t_win.txt", "r");
    n = fwscanf(f, L"%d%d", &a, &b);
    wprintf(L"fwscanf %d: %d %d\n", n, a, b);
    fclose(f);
    remove("t_win.txt");

    return 0;
}
