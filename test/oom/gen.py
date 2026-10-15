#!/usr/bin/env python3
"""gen.py <axis> <n>: a C program that stresses one of acc's memory axes at
size n, for test/oom.sh. Each axis is one thing a compile holds in memory
while it works: functions, one function's statements, globals, strings,
macros, a struct's members, a switch's cases, nested blocks, locals, one
long expression, and a program reaching much of the library."""
import sys

axis, n = sys.argv[1], int(sys.argv[2])
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
else:
    sys.exit(f'no axis {axis}')
print('\n'.join(out))
