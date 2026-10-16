#!/usr/bin/env python3
"""gen.py <axis> <n>: a C program that stresses one of acc's memory axes at
size n, for test/oom.sh. Each axis is one thing a compile holds in memory
while it works: functions, one function's statements, globals, strings,
macros, a struct's members, a switch's cases, nested blocks, locals, one
long expression, a program reaching much of the library, and `reach`: a
little code that calls n different functions of it."""
import sys

axis, n = sys.argv[1], int(sys.argv[2])

# reach's calls: each a different function of the library, C's and the Agon's.
CALLS = ['d += sin(d)', 'd += cos(d)', 'd += tan(d)', 'd += asin(0.5)', 'd += acos(0.5)', 'd += atan(d)', 'd += atan2(d, 2.0)', 'd += sinh(d)', 'd += cosh(d)', 'd += tanh(d)', 'd += exp(d)', 'd += log(2.0)', 'd += log10(2.0)', 'd += pow(d, 2.0)', 'd += sqrt(2.0)', 'd += ceil(d)', 'd += floor(d)', 'd += fabs(d)', 'd += fmod(d, 3.0)', 'd += frexp(d, &i)', 'd += ldexp(d, 2)', 'd += modf(d, &e)', 'd += cbrt(d)', 'd += hypot(d, 2.0)', 'd += expm1(d)', 'd += log1p(d)', 'd += log2(2.0)', 'd += exp2(d)', 'd += trunc(d)', 'd += round(d)', 'n += lround(d)', 'd += rint(d)', 'd += nearbyint(d)', 'd += remainder(d, 3.0)', 'd += fdim(d, 1.0)', 'd += fmax(d, 1.0)', 'd += fmin(d, 1.0)', 'd += fma(d, 2.0, 1.0)', 'd += erf(d)', 'd += erfc(d)', 'd += tgamma(2.5)', 'd += lgamma(2.5)', 'd += asinh(d)', 'd += acosh(2.0)', 'd += atanh(0.5)', 'd += copysign(d, -1.0)', 'd += nextafter(d, 2.0)', 'd += logb(d)', 'n += ilogb(8.0)', 'd += scalbn(d, 2)', 'n += strlen(buf)', 'strcpy(buf, "abc")', 'strncpy(buf, "abcd", 3)', 'strcat(buf, "x")', 'strncat(buf, "yz", 1)', 'n += strcmp(buf, "a")', 'n += strncmp(buf, "a", 1)', 'n += strchr(buf, 98) != 0', 'n += strrchr(buf, 98) != 0', 'n += strstr(buf, "b") != 0', 'n += strspn(buf, "ab")', 'n += strcspn(buf, "c")', 'n += strpbrk(buf, "c") != 0', 'n += strtok(buf, ",") != 0', 'memcpy(buf, "q", 1)', 'memmove(buf + 1, buf, 2)', 'memset(buf + 8, 0, 4)', 'n += memcmp(buf, "q", 1)', 'n += memchr(buf, 113, 4) != 0', 'free(strdup(buf))', 'n += strerror(2)[0]', 'n += strcasecmp(buf, "Q")', 'n += atoi("12")', 'n += atol("12")', 'n += strtol("12", 0, 10)', 'n += strtoul("12", 0, 10)', 'd += strtod("1.5", 0)', 'n += abs(-n)', 'n += labs(-3L)', 'n += div(7, 2).quot', 'n += rand()', 'srand(1)', 'qsort(buf, 3, 1, cmp)', 'n += bsearch(buf, buf, 3, 1, cmp) != 0', 'free(malloc(8))', 'free(calloc(2, 4))', 'free(realloc(0, 8))', 'printf("%d\\n", n)', 'sprintf(buf, "%d", n)', 'snprintf(buf, 8, "%x", n)', 'n += sscanf("4", "%d", &i)', 'f = fopen("t.txt", "w")', 'fputs("a\\n", f)', 'fprintf(f, "%d\\n", n)', 'fwrite(buf, 1, 2, f)', 'n += ftell(f)', 'fseek(f, 0, SEEK_SET)', 'fclose(f)', 'f = fopen("t.txt", "r")', 'n += fgets(buf, 8, f) != 0', 'n += fread(buf, 1, 2, f)', 'fclose(f)', 'rename("t.txt", "u.txt")', 'remove("u.txt")', 'puts(buf)', 'putchar(65)', 'n += isalpha(65)', 'n += isdigit(48)', 'n += isspace(32)', 'n += toupper(97)', 'n += tolower(65)', 'now = time(0)', 'n += gmtime(&now)->tm_year', 'n += (int) mktime(gmtime(&now))', 'n += strftime(buf, 32, "%Y", gmtime(&now))', 'n += (int) clock()', 'd += difftime(now, now)', 'n += wcslen(L"ab")', 'wcscpy(wb, L"x")', 'n += mbstowcs(wb, "ab", 4)', 'n += wcstombs(buf, wb, 8)', 'n += swprintf(wb, 4, L"%d", 1)', 'vdp_clear_screen()', 'mos_putstring("x")', 'vdp_mode(1)', 'vdp_cursor_tab(1, 1)', 'vdp_set_text_colour(1)', 'vdp_plot(69, 10, 10)', 'vdp_move_to(1, 1)', 'vdp_line_to(5, 5)', 'vdp_set_graphics_colour(0, 1)']
out = []
w = out.append

if axis == 'functions':            # many small functions, each called
    w('#include <stdio.h>')
    for i in range(n):
        w(f'int f{i}(int x) {{ return x * {i % 7 + 1} + {i}; }}')
    w('int main(void) { int s = 0;')
    for i in range(n):
        w(f'    s += f{i}({i % 5});')
    w('    printf("%d\\n", s); return 0; }')
elif axis == 'longfunc':           # one function of n statements
    w('#include <stdio.h>')
    w('int main(int argc, char **argv) { int a = argc, b = 1, c = 2;')
    for i in range(n):
        w(f'    if (a > {i % 13}) b += c * {i % 9 + 1}; else c -= b;')
    w('    printf("%d %d\\n", b, c); return 0; }')
elif axis == 'globals':            # n initialised globals
    w('#include <stdio.h>')
    for i in range(n):
        w(f'int g{i} = {i * 3};')
    w('int main(void) { long s = 0;')
    for i in range(0, n, max(1, n // 50)):
        w(f'    s += g{i};')
    w('    printf("%ld\\n", s); return 0; }')
elif axis == 'strings':            # n distinct string literals
    w('#include <stdio.h>')
    w('static const char *const t[] = {')
    for i in range(n):
        w(f'    "string number {i} of the table",')
    w('};')
    w(f'int main(void) {{ printf("%s\\n", t[{n - 1}]); return 0; }}')
elif axis == 'macros':             # n object-like macros
    w('#include <stdio.h>')
    for i in range(n):
        w(f'#define MACRO_NUMBER_{i} ({i} + 1)')
    w(f'int main(void) {{ printf("%d\\n", MACRO_NUMBER_{n - 1}); return 0; }}')
elif axis == 'members':            # one struct of n members
    w('#include <stdio.h>')
    w('struct big {')
    for i in range(n):
        w(f'    int member_{i};')
    w('};')
    w('static struct big b;')
    w('int main(void) { b.member_%d = 5; printf("%%d\\n", b.member_%d); return 0; }' % (n - 1, n - 1))
elif axis == 'switch':             # one switch of n cases
    w('#include <stdio.h>')
    w('int pick(int x) { switch (x) {')
    for i in range(n):
        w(f'    case {i * 3}: return {i};')
    w('    } return -1; }')
    w(f'int main(void) {{ printf("%d\\n", pick({(n - 1) * 3})); return 0; }}')
elif axis == 'nesting':            # n nested blocks
    w('#include <stdio.h>')
    w('int main(int argc, char **argv) { int x = argc;')
    for i in range(n):
        w('    ' + ' ' * (i % 40) + f'if (x < {i + 100}) {{ x++;')
    w('    ' + '}' * n)
    w('    printf("%d\\n", x); return 0; }')
elif axis == 'locals':             # one function with n locals
    w('#include <stdio.h>')
    w('int main(int argc, char **argv) {')
    for i in range(n):
        w(f'    int v{i} = argc + {i};')
    w(f'    printf("%d\\n", v0 + v{n - 1}); return 0; }}')
elif axis == 'expr':               # one expression n terms long
    w('#include <stdio.h>')
    w('int main(int argc, char **argv) { int x = argc; int y = ')
    w('    ' + ' + '.join(f'(x * {i % 7 + 1})' for i in range(n)) + ';')
    w('    printf("%d\\n", y); return 0; }')
elif axis == 'libcalls':           # a program that reaches much of the library
    w('#include <stdio.h>')
    w('#include <stdlib.h>')
    w('#include <string.h>')
    w('#include <ctype.h>')
    w('#include <math.h>')
    w('#include <time.h>')
    w('#include <wchar.h>')
    w('#include <agon/mos.h>')
    w('#include <agon/vdp.h>')
    w('int main(int argc, char **argv) {')
    w('    char buf[64]; FILE *f = fopen("t.txt", "w"); wchar_t wb[8];')
    w('    double d = strtod("2.5", NULL); time_t now = time(NULL);')
    w('    fprintf(f, "%d %f %s\\n", argc, sqrt(d) + sin(d) + pow(d, 2.0), argv[0]);')
    w('    fclose(f); f = fopen("t.txt", "r"); fgets(buf, sizeof buf, f); fclose(f);')
    w('    sscanf(buf, "%d", &argc); qsort(buf, 4, 1, (int (*)(const void *, const void *)) strcmp);')
    w('    mbstowcs(wb, "ab", 8); swprintf(wb, 8, L"%d", 1);')
    w('    printf("%s %ld %d\\n", strftime(buf, 64, "%Y", gmtime(&now)) ? buf : "", (long) labs(-3), toupper(\'a\'));')
    w('    vdp_clear_screen(); mos_putstring("x");')
    for i in range(n):
        w(f'    snprintf(buf, sizeof buf, "%d %x %s", {i}, {i}, strchr(buf, {i % 90 + 32}) ? "y" : "n");')
    w('    return 0; }')
elif axis == 'reach':                # little code, much of the library
    for h in ('stdio', 'stdlib', 'string', 'ctype', 'math', 'time', 'wchar'):
        w(f'#include <{h}.h>')
    w('#include <agon/mos.h>')
    w('#include <agon/vdp.h>')
    w('static int cmp(const void *a, const void *b) { return *(const char *) a - *(const char *) b; }')
    w('int main(void) {')
    w('    double d = 0.5, e; int i, n = 0; char buf[64] = "abc"; wchar_t wb[8]; FILE *f; time_t now;')
    for c in CALLS[:n]:
        w(f'    {c};')
    w('    return n + (int) d; }')
else:
    sys.exit(f'no axis {axis}')
print('\n'.join(out))
