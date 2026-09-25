#!/usr/bin/env python3
"""Writes the benchmark inputs that cover what the older ones do not.

    test/bench/generate.py            # rewrites pointers.c, operators.c,
                                      # data.c, jumps.c, matrix.c, text.c,
                                      # records.c, linkage.c, features.c,
                                      # preproc.c, declare.c and numeric.c

The eight older inputs were written before acc had pointers, the logical
operators, ++ and --, ?:, compound assignment, hex and octal constants,
for loops, globals or arrays, and none of them uses any of those -- so the
benchmark measured all of it as costing nothing, which is the mistake
control.c was written to stop happening for if and while. These three are
the same size as the others, at around 17 KB, with names the length real C
has (see names.c), and each returns 42.

The outputs are committed, as the others are, so that a number from today
can be compared with one from last week; this is here so that they can be
made again rather than edited by hand. What main has to subtract to land on
42 is found by compiling the program with the host's C compiler and running
it: every value stays far from the edge of a 24-bit int, where the host's
32 bits and the Agon's 24 would disagree.
"""

import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))

NOUNS = ["anchor", "buffer", "cursor", "header", "margin", "marker",
         "offset", "record", "result", "stride", "bucket", "origin",
         "prefix", "window", "column", "credit", "ledger", "sample",
         "signal", "target"]


def names(count):
    """count distinct nouns, cycling through the list with a number when it
    runs out."""
    out = []
    for i in range(count):
        noun = NOUNS[i % len(NOUNS)]
        out.append(noun if i < len(NOUNS) else f"{noun}{i // len(NOUNS)}")
    return out


# ---------------------------------------------------------------- pointers

def pointers(fn_count):
    parts = []
    calls = []
    for i, noun in enumerate(names(fn_count)):
        k = i % 7
        kind = i % 7
        if kind == 0:
            parts.append(f"""int swap_{noun}(int *left, int *right) {{
    int held = *left;

    *left = *right;
    *right = held;

    return *left - *right + {k};
}}
""")
            calls.append(f"    total = total + swap_{noun}(&left, &right);")
        elif kind == 1:
            parts.append(f"""int clamp_{noun}(int *slot, int limit) {{
    if (*slot > limit)
        *slot = limit;

    return *slot + {k};
}}
""")
            calls.append(f"    total = total + clamp_{noun}(&left, {5 + k});")
        elif kind == 2:
            parts.append(f"""int total_{noun}(int *first, int count) {{
    int total = 0;
    int *cursor = first;

    while (count) {{
        total = total + *cursor;
        cursor = cursor + 1;
        count = count - 1;
    }}

    return total - {k};
}}
""")
            calls.append(f"    total = total + total_{noun}(table, {1 + k});")
        elif kind == 3:
            parts.append(f"""int span_{noun}(char *start, char *end) {{
    char *probe = start;
    int steps = 0;

    while (probe != end) {{
        steps = steps + *probe;
        probe = probe + 1;
    }}

    return steps + (end - start);
}}
""")
            calls.append(f"    total = total + span_{noun}(text, text + {2 + k});")
        elif kind == 4:
            parts.append(f"""int adjust_{noun}(int **handle, int value) {{
    int *inner = *handle;

    **handle = **handle + value;
    *inner = *inner - {k};

    return **handle;
}}
""")
            calls.append(f"    total = total + adjust_{noun}(&where, {3 + k});")
        elif kind == 5:
            parts.append(f"""int bump_{noun}(char *field, short *half, int amount) {{
    *field = *field + amount;
    *half = *half - *field;

    return *field + *half;
}}
""")
            calls.append(f"    total = total + bump_{noun}(&small, &half, {k});")
        else:
            parts.append(f"""int widen_{noun}(short *half, long *whole) {{
    long *spare = whole;

    *spare = *whole + *half;
    if (*whole > 1000)
        *whole = *whole - 1000;

    return *half + {k};
}}
""")
            calls.append(f"    total = total + widen_{noun}(&half, &whole);")

    main_head = """int main(void) {
    int left = 3;
    int right = 11;
    int table[8];
    char text[12];
    char small = 5;
    short half = 70;
    long whole = 100;
    int *where = &right;
    int total = 0;
    int i = 0;

    while (i < 8) {
        table[i] = i * 3 + 1;
        i = i + 1;
    }
    i = 0;
    while (i < 12) {
        text[i] = i;
        i = i + 1;
    }
"""
    return parts, main_head, calls, "total"


# ---------------------------------------------------------------- operators

def operators(fn_count):
    parts = []
    calls = []
    for i, noun in enumerate(names(fn_count)):
        k = i % 9
        kind = i % 6
        if kind == 0:
            parts.append(f"""int either_{noun}(int left, int right) {{
    if (left > {k} && right < {k + 20} || left == right)
        return 1 + !right;

    return !left || right > {k};
}}
""")
            calls.append(f"    total = total + either_{noun}({k + 1}, {k + 2});")
        elif kind == 1:
            parts.append(f"""int pick_{noun}(int value, int limit) {{
    int wide = value > limit ? value - limit : limit - value;

    return wide > {k} ? wide : value < 0 ? -value : {k};
}}
""")
            calls.append(f"    total = total + pick_{noun}({k * 3}, {k + 4});")
        elif kind == 2:
            parts.append(f"""int count_{noun}(int value) {{
    int steps = 0;
    int rest = value;

    while (value-- > 0)
        steps++;
    --rest;
    ++steps;

    return steps + rest--;
}}
""")
            calls.append(f"    total = total + count_{noun}({k + 2});")
        elif kind == 3:
            parts.append(f"""int mix_{noun}(int value) {{
    int sum = {k + 10};

    sum += value;
    sum -= 3;
    sum *= 2;
    sum /= 3;
    sum %= 1000;
    sum <<= 2;
    sum >>= 1;
    sum &= 0x7ff;
    sum |= 010;
    sum ^= 0x55;

    return sum;
}}
""")
            calls.append(f"    total = total + mix_{noun}({k * 5});")
        elif kind == 4:
            parts.append(f"""int flags_{noun}(unsigned int mask) {{
    unsigned int low = mask & 0x0f;

    if (low != 0 && !(mask & 0x100))
        return 0x10 + low;

    return mask & 0200 ? 020 : 07;
}}
""")
            calls.append(f"    total = total + flags_{noun}(0x{0x21 + k * 0x13:x});")
        else:
            parts.append(f"""int tally_{noun}(int start, int step) {{
    int count = 0;
    int limit = start + {k + 6};

    while (start < limit && count < 20) {{
        count += start % 2 == 0 || step > 3 ? 2 : 1;
        start += step;
    }}

    return count;
}}
""")
            calls.append(f"    total = total + tally_{noun}({k}, {1 + k % 4});")

    main_head = """int main(void) {
    int total = 0;
"""
    return parts, main_head, calls, "total"


# ---------------------------------------------------------------- data

def data(fn_count):
    parts = []
    calls = []
    for i, noun in enumerate(names(fn_count)):
        k = i % 8
        kind = i % 5
        if kind == 0:
            parts.append(f"""int table_{noun}[8] = {{{", ".join(str((j * 3 + k) % 11) for j in range(8))}}};
int count_{noun};

int scan_{noun}(int limit) {{
    int found = 0;

    for (int i = 0; i < 8; i++)
        if (table_{noun}[i] > limit)
            found = found + 1;
    count_{noun} = count_{noun} + found;

    return found + count_{noun};
}}
""")
            calls.append(f"    total = total + scan_{noun}({k % 6});")
        elif kind == 1:
            parts.append(f"""int fill_{noun}(int start) {{
    int items[6];
    int sum = 0;

    for (int i = 0; i < 6; i++)
        items[i] = start + i;
    for (int i = 5; i >= 0; i = i - 1)
        sum = sum + items[i];

    return sum - {k};
}}
""")
            calls.append(f"    total = total + fill_{noun}({k});")
        elif kind == 2:
            parts.append(f"""char marks_{noun}[16];

int mark_{noun}(int step) {{
    int i;

    for (i = 0; i < 16; i = i + step)
        marks_{noun}[i] = i;

    return marks_{noun}[step] + i;
}}
""")
            calls.append(f"    total = total + mark_{noun}({1 + k % 4});")
        elif kind == 3:
            parts.append(f"""long weights_{noun}[] = {{{", ".join(str(1000 * (j + 1) + k) for j in range(5))}}};

int weigh_{noun}(int index) {{
    long sum = 0;

    for (int i = 0; i < 5; i = i + 1)
        sum = sum + weights_{noun}[i];
    weights_{noun}[index] = weights_{noun}[index] + {k};

    return sum / 1000 + index;
}}
""")
            calls.append(f"    total = total + weigh_{noun}({k % 5});")
        else:
            parts.append(f"""int level_{noun} = {k + 1};

int shift_{noun}(int amount) {{
    short cells[4] = {{{k}, {k + 1}}};
    int sum = 0;

    for (int i = 0; i < 4; i = i + 1)
        sum = sum + cells[i] * level_{noun};
    level_{noun} = level_{noun} + amount;

    return sum;
}}
""")
            calls.append(f"    total = total + shift_{noun}({k});")

    main_head = """int main(void) {
    int total = 0;
"""
    return parts, main_head, calls, "total"


# ---------------------------------------------------------------- jumps

def jumps(fn_count):
    parts = []
    calls = []
    for i, noun in enumerate(names(fn_count)):
        k = i % 7
        kind = i % 6
        if kind == 0:
            parts.append(f"""int route_{noun}(int code) {{
    int cost = 0;

    switch (code % 6) {{
    case 0:
        cost += 3;
    case 1:
        cost += 5;
        break;
    case 2:
    case 3:
        cost = code * 2;
        break;
    default:
        cost = -1;
    }}

    return cost + {k};
}}
""")
            calls.append(f"    total = total + route_{noun}({k + i});")
        elif kind == 1:
            parts.append(f"""int sparse_{noun}(long key) {{
    switch (key) {{
    case 100000:
        return 1;
    case -7:
        return 2;
    case 65536:
        return 3;
    case {40 + k}:
        return 4;
    default:
        return {k};
    }}
}}
""")
            key = [100000, -7, 65536, 40 + k, 9][i % 5]
            calls.append(f"    total = total + sparse_{noun}({key});")
        elif kind == 2:
            parts.append(f"""int drain_{noun}(int level) {{
    int steps = 0;

    do {{
        level = level - 3;
        if (level % 2 == 0)
            continue;
        steps++;
        if (steps > {k + 3})
            break;
    }} while (level > 0);

    return steps;
}}
""")
            calls.append(f"    total = total + drain_{noun}({20 + k});")
        elif kind == 3:
            parts.append(f"""int search_{noun}(int target) {{
    int found = -1;
    int i;

    for (i = 0; i < 20; i++) {{
        if (i % 3 == 0)
            continue;
        if (i * {k + 1} >= target) {{
            found = i;
            break;
        }}
    }}
    while (found > 10) {{
        found = found - 4;
        if (found < 12)
            break;
    }}

    return found;
}}
""")
            calls.append(f"    total = total + search_{noun}({5 + k * 3});")
        elif kind == 4:
            source = ", ".join(str((j * 5 + k) % 23 + 1) for j in range(24))
            parts.append(f"""char dest_{noun}[24];
char source_{noun}[24] = {{{source}}};

int send_{noun}(int count) {{
    char *to = dest_{noun};
    char *from = source_{noun};
    int n = (count + 7) / 8;

    switch (count % 8) {{
    case 0: do {{ *to++ = *from++;
    case 7:      *to++ = *from++;
    case 6:      *to++ = *from++;
    case 5:      *to++ = *from++;
    case 4:      *to++ = *from++;
    case 3:      *to++ = *from++;
    case 2:      *to++ = *from++;
    case 1:      *to++ = *from++;
            }} while (--n > 0);
    }}

    return dest_{noun}[count - 1];
}}
""")
            calls.append(f"    total = total + send_{noun}({3 + i % 18});")
        else:
            parts.append(f"""int tally_{noun}(int limit) {{
    int total = 0;

    for (int i = 0; i < limit; i++) {{
        switch (i & 3) {{
        case 0:
            continue;
        case 1:
            total += i;
            break;
        case 2:
            total -= 1;
            break;
        default:
            total += 2;
        }}
        total++;
    }}

    return total;
}}
""")
            calls.append(f"    total = total + tally_{noun}({6 + k});")

    main_head = """int main(void) {
    int total = 0;
"""
    return parts, main_head, calls, "total"


# ---------------------------------------------------------------- matrix

def matrix(fn_count):
    parts = []
    calls = []
    for i, noun in enumerate(names(fn_count)):
        k = i % 7
        kind = i % 5
        if kind == 0:
            parts.append(f"""int grid_{noun}[4][5];

void fill_{noun}(int seed) {{
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 5; j++)
            grid_{noun}[i][j] = seed + i * j;
}}
""")
            calls.append(f"    fill_{noun}({k});\n"
                         f"    total = total + grid_{noun}[3][4];")
        elif kind == 1:
            parts.append(f"""int trace_{noun}(int bias) {{
    int m[3][3] = {{{{bias, 1, 2}}, {{3, bias, 4}}, {{5, 6, bias}}}};
    int (*row)[3] = m;
    int sum = 0;

    for (int i = 0; i < 3; i++) {{
        sum += row[i][i];
        sum += (*(row + i))[0];
    }}

    return sum;
}}
""")
            calls.append(f"    total = total + trace_{noun}({k});")
        elif kind == 2:
            parts.append(f"""int locate_{noun}(int target) {{
    int cells[3][4] = {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}};
    int found = -1;

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            if (cells[i][j] == target) {{
                found = i * 4 + j;
                goto done;
            }}
done:
    return found;
}}
""")
            calls.append(f"    total = total + locate_{noun}({2 + k});")
        elif kind == 3:
            parts.append(f"""void bump_{noun}(int (*plane)[2], int by) {{
    for (int i = 0; i < 2; i++) {{
        plane[i][0] += by;
        plane[i][1] -= by;
    }}
}}

int layer_{noun}(int by) {{
    int box[2][2][2] = {{{{{{1, 2}}, {{3, 4}}}}, {{{{5, 6}}, {{7, 8}}}}}};

    bump_{noun}(box[1], by);

    return box[1][0][0] + box[1][1][1] + box[0][1][0];
}}
""")
            calls.append(f"    total = total + layer_{noun}({k});")
        else:
            ring = ", ".join(str((j * 3 + k) % 10) for j in range(6))
            parts.append(f"""int ring_{noun}[6] = {{{ring}}};

void rotate_{noun}(int (*all)[6]) {{
    int first = (*all)[0];

    for (int i = 0; i < 5; i++)
        (*all)[i] = (*all)[i + 1];
    (*all)[5] = first;
}}
""")
            calls.append(f"    rotate_{noun}(&ring_{noun});\n"
                         f"    total = total + ring_{noun}[0];")

    main_head = """int main(void) {
    int total = 0;
"""
    return parts, main_head, calls, "total"


# ---------------------------------------------------------------- text

def text(fn_count):
    parts = []
    calls = []
    words = ["alpha", "beta", "gamma", "delta", "omega", "sigma"]
    for i, noun in enumerate(names(fn_count)):
        k = i % 7
        kind = i % 5
        if kind == 0:
            parts.append(f"""int count_{noun}(char *text, char mark) {{
    int count = 0;

    while (*text) {{
        if (*text == mark || *text == '\\t')
            count++;
        text++;
    }}

    return count;
}}
""")
            calls.append(f'    total = total + count_{noun}("a,b,,c\\td{k}", \',\');')
        elif kind == 1:
            parts.append(f"""int kind_{noun}(char c) {{
    if (c >= '0' && c <= '9')
        return 1;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        return 2;
    switch (c) {{
    case ' ':
    case '\\t':
    case '\\n':
        return 3;
    case '\\\\':
        return 4;
    }}

    return {k};
}}
""")
            calls.append(f"    total = total + kind_{noun}('7') + kind_{noun}('q') "
                         f"+ kind_{noun}('\\t') + kind_{noun}('#');")
        elif kind == 2:
            parts.append(f"""int copy_{noun}(int shift) {{
    char source[] = "the quick brown fox";
    char target[24];
    int n = 0;

    for (int i = 0; source[i]; i++) {{
        char c = source[i];

        if (c != ' ') {{
            int upper = c - 'a' + 'A' + shift;

            target[n++] = upper;
        }}
    }}
    target[n] = '\\0';

    return n + target[{k}] - 'A';
}}
""")
            calls.append(f"    total = total + copy_{noun}({k % 3});")
        elif kind == 3:
            table = ", ".join(f'"{words[(j + k) % 6]}"' for j in range(4))
            key = words[(k + 2) % 6]
            parts.append(f"""char words_{noun}[4][8] = {{{table}}};

int find_{noun}(char *key) {{
    for (int i = 0; i < 4; i++) {{
        char *word = words_{noun}[i];
        int j = 0;

        while (word[j] && word[j] == key[j])
            j++;
        if (word[j] == key[j])
            return i;
    }}

    return -1;
}}
""")
            calls.append(f'    total = total + find_{noun}("{key}") + find_{noun}("kappa");')
        else:
            parts.append(f"""int nest_{noun}(int x) {{
    int sum = x;

    {{
        int x = sum * 2;

        {{
            int x = {k + 3};

            sum += x;
        }}
        sum += x;
    }}

    return sum + 'a' - 'A';
}}
""")
            calls.append(f"    total = total + nest_{noun}({k});")

    main_head = """int main(void) {
    int total = 0;
"""
    return parts, main_head, calls, "total"


# ---------------------------------------------------------------- records

def records(fn_count):
    parts = []
    calls = []
    for i, noun in enumerate(names(fn_count)):
        k = i % 7
        kind = i % 5
        if kind == 0:
            parts.append(f"""typedef struct {{
    int count;
    unsigned char flags;
    long weight;
    char label[6];
}} {noun}_t;

{noun}_t {noun}_table[4] = {{
    {{ 1, 2, 30000, "one" }},
    {{ 2, 4, 40000, "two" }},
    {{ 3 }},
    {{ 4, 8 }}
}};

int weigh_{noun}({noun}_t *entry, int scale) {{
    int total = 0;

    for (int i = 0; i < sizeof {noun}_table / sizeof {noun}_table[0]; i++) {{
        entry[i].count += scale;
        entry[i].flags |= {k};
        total += entry[i].count + entry[i].flags + (entry[i].weight > 35000);
        total += entry[i].label[0] == 't';
    }}

    return total;
}}
""")
            calls.append(f"    total = total + weigh_{noun}({noun}_table, {k});")
        elif kind == 1:
            parts.append(f"""enum shape_{noun} {{ CIRCLE_{noun}, SQUARE_{noun}, TRIANGLE_{noun} = 5 }};

struct figure_{noun} {{
    enum shape_{noun} shape;
    union {{
        int radius;
        struct {{ short width, height; }} box;
        char sides[3];
    }} size;
}};

int area_{noun}(struct figure_{noun} *figure) {{
    switch (figure->shape) {{
    case CIRCLE_{noun}:
        return 3 * figure->size.radius * figure->size.radius;
    case SQUARE_{noun}:
        return figure->size.box.width * figure->size.box.height;
    case TRIANGLE_{noun}:
        return figure->size.sides[0] + figure->size.sides[1] + figure->size.sides[2];
    }}

    return {k};
}}

int shapes_{noun}(int n) {{
    struct figure_{noun} figures[3];

    figures[0].shape = CIRCLE_{noun};
    figures[0].size.radius = n;
    figures[1].shape = SQUARE_{noun};
    figures[1].size.box.width = n + 1;
    figures[1].size.box.height = {k + 2};
    figures[2].shape = TRIANGLE_{noun};
    figures[2].size.sides[0] = 3;
    figures[2].size.sides[1] = 4;
    figures[2].size.sides[2] = n;

    return area_{noun}(&figures[0]) + area_{noun}(&figures[1])
           + area_{noun}(figures + 2);
}}
""")
            calls.append(f"    total = total + shapes_{noun}({k + 1});")
        elif kind == 2:
            parts.append(f"""struct span_{noun} {{ int start, end; char open; }};

struct span_{noun} widen_{noun}(struct span_{noun} span, int by) {{
    span.start -= by;
    span.end += by;
    span.open = span.end - span.start > {k + 5};

    return span;
}}

int measure_{noun}(struct span_{noun} span) {{
    return span.end - span.start + span.open;
}}

int spans_{noun}(int first) {{
    struct span_{noun} span = {{ first, first + {k} }};
    struct span_{noun} copy;

    span = widen_{noun}(span, 2);
    copy = span;
    copy.end++;

    return measure_{noun}(widen_{noun}(copy, 1)) + measure_{noun}(span);
}}
""")
            calls.append(f"    total = total + spans_{noun}({k});")
        elif kind == 3:
            parts.append(f"""struct link_{noun} {{
    int value;
    struct link_{noun} *next;
    struct {{ int hits; char seen; }} stats;
}};

struct link_{noun} chain_{noun}[5];

int walk_{noun}(int start) {{
    struct link_{noun} *at;
    int sum = 0;

    for (int i = 0; i < 5; i++) {{
        chain_{noun}[i].value = start + i;
        chain_{noun}[i].next = i < 4 ? &chain_{noun}[i + 1] : 0;
    }}
    for (at = chain_{noun}; at; at = at->next) {{
        at->stats.hits++;
        at->stats.seen = 1;
        sum += at->value * at->stats.seen;
    }}

    return sum + chain_{noun}[0].next->next->value;
}}
""")
            calls.append(f"    total = total + walk_{noun}({k});")
        else:
            parts.append(f"""typedef unsigned char byte_{noun};
typedef short half_{noun};
typedef byte_{noun} *cursor_{noun};

int pack_{noun}(int value) {{
    byte_{noun} bytes[4];
    cursor_{noun} at = bytes;
    half_{noun} low = (half_{noun}) (value * {k + 3});

    bytes[0] = (byte_{noun}) value;
    bytes[1] = (byte_{noun}) (value + 1);
    bytes[2] = (byte_{noun}) low;
    bytes[3] = (byte_{noun}) sizeof(byte_{noun});

    return at[0] + at[1] + at[2] + at[3] + (int) (long) low;
}}
""")
            calls.append(f"    total = total + pack_{noun}({k + 10});")

    main_head = """int main(void) {
    int total = 0;
"""
    return parts, main_head, calls, "total"


# ---------------------------------------------------------------- linkage

def linkage(fn_count):
    parts = []
    calls = []
    protos = []
    for i, noun in enumerate(names(fn_count)):
        k = i % 7
        kind = i % 4
        if kind == 0:
            protos.append(f"static long scale_{noun}(const int factor, long base);")
            parts.append(f"""static long scale_{noun}(const int factor, long base) {{
    const long limit = 900000;
    long result = base * factor;

    return result > limit ? limit : result;
}}
""")
            calls.append(f"    total = total + (int) (scale_{noun}({k + 2}, 1000L) / 100);")
        elif kind == 1:
            protos.append(f"int count_{noun}(void);")
            parts.append(f"""int count_{noun}(void) {{
    static int calls;
    static const char steps[4] = {{ 1, 2, 3, {k} }};

    calls++;

    return calls * 10 + steps[calls & 3];
}}
""")
            calls.append(f"    total = total + count_{noun}() + count_{noun}();")
        elif kind == 2:
            protos.append(f"extern int shared_{noun};")
            protos.append(f"int peek_{noun}(const int *const cells, int n);")
            parts.append(f"""int shared_{noun} = {k + 4};

int peek_{noun}(const int *const cells, int n) {{
    extern int shared_{noun};
    int sum = shared_{noun};

    for (int i = 0; i < n; i++)
        sum += cells[i];

    return sum;
}}
""")
            calls.append(f"    {{ int cells[3] = {{ {k}, 2, 3 }}; total = total + peek_{noun}(cells, 3); }}")
        else:
            protos.append(f"char grade_{noun}(unsigned char mark);")
            parts.append(f"""char grade_{noun}(unsigned char mark) {{
    static const unsigned char bands[3] = {{ 90, 70, 50 }};
    const char *letters = "ABCD";

    for (int i = 0; i < 3; i++)
        if (mark >= bands[i])
            return letters[i];

    return letters[3];
}}
""")
            calls.append(f"    total = total + grade_{noun}({40 + 10 * k}) - 'A';")

    main_head = "\n".join(protos) + """

int main(void) {
    int total = 0;
"""
    return parts, main_head, calls, "total"


# ---------------------------------------------------------------- features

def features(fn_count):
    parts = []
    calls = []
    for i, noun in enumerate(names(fn_count)):
        k = i % 7
        kind = i % 6
        if kind == 0:
            parts.append(f"""struct flags_{noun} {{
    unsigned int ready : 1;
    unsigned int level : 3;
    signed int offset : 6;
    unsigned int count : 12;
    _Bool seen : 1;
}};

int pack_{noun}(int level, int offset) {{
    struct flags_{noun} state;

    state.ready = 1;
    state.level = level & 7;
    state.offset = offset;
    state.count = {k} * 100;
    state.seen = offset != 0;

    return state.level + state.offset + (state.count >> 4) + state.seen;
}}
""")
            calls.append(f"    total = total + pack_{noun}({k}, {k - 3});")
        elif kind == 1:
            parts.append(f"""static int twice_{noun}(int value) {{ return value * 2; }}
static int less_{noun}(int value) {{ return value - {k + 1}; }}

int apply_{noun}(int value, int which) {{
    int (*const steps[2])(int) = {{ twice_{noun}, less_{noun} }};
    int (*step)(int) = steps[which & 1];

    return step(value) + (*steps[1])(value);
}}
""")
            calls.append(f"    total = total + apply_{noun}({k}, {k});")
        elif kind == 2:
            parts.append(f"""int gather_{noun}(int count, ...) {{
    va_list args;
    int sum = 0;

    va_start(args, count);
    while (count-- > 0)
        sum = sum + va_arg(args, int);
    va_end(args);

    return sum;
}}
""")
            calls.append(f"    total = total + gather_{noun}(3, {k}, {k + 1}, {k + 2});")
        elif kind == 3:
            parts.append(f"""long long wider_{noun}(long long base, int shift) {{
    long long scaled = base << shift;
    unsigned long long mask = 0xffffffffULL;

    if ((scaled & mask) > base)
        scaled = scaled - base;

    return scaled / ({k} + 3);
}}
""")
            calls.append(f"    total = total + (int) (wider_{noun}({k + 1}00000LL, "
                         f"{k % 5}) % 97);")
        elif kind == 4:
            parts.append(f"""_Bool ready_{noun}(const volatile int *const cell, register int limit) {{
    _Bool over = *cell > limit;
    _Bool under = !over && *cell < {k};

    return over || under;
}}
""")
            calls.append(f"    {{ volatile int cell = {k * 3}; "
                         f"total = total + ready_{noun}(&cell, {k}); }}")
        else:
            parts.append(f"""struct span_{noun} {{ int lo, hi; }};

struct span_{noun} widest_{noun}(struct span_{noun} left, struct span_{noun} right) {{
    return left.hi - left.lo > right.hi - right.lo ? left : right;
}}

int span_{noun}(int base) {{
    struct span_{noun} a = {{ base, base + {k} }}, b = {{ base, base + 3 }};

    return widest_{noun}(a, b).hi - base;
}}
""")
            calls.append(f"    total = total + span_{noun}({k});")

    main_head = """int main(void) {
    int total = 0;
"""
    return parts, main_head, calls, "total"


def preproc(fn_count):
    """Each function with macros of its own: constants, function-like ones
    nested in each other, # and ##, a variadic one, one spread over lines
    with a backslash, one taken back with #undef and given again, and
    #if/#elif/#ifdef/#ifndef over them -- the preprocessor, which no
    generated input had a line of."""
    parts = []
    calls = []
    for i, noun in enumerate(names(fn_count)):
        k = i % 7
        kind = i % 6
        up = noun.upper() + str(i)
        if kind == 0:
            parts.append(f"""#define LIMIT_{up} ({k} + 12)
#define SQUARE_{up}(x) ((x) * (x))
#define CLAMP_{up}(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))

int scale_{noun}(int value) {{
    int squared = SQUARE_{up}(value % 7);

    return CLAMP_{up}(squared, 1, LIMIT_{up}) + LIMIT_{up} / 2;
}}
""")
            calls.append(f"    total = total + scale_{noun}({k + 4});")
        elif kind == 1:
            parts.append(f"""#define MODE_{up} {k % 3}
#if MODE_{up} == 0
# define STEP_{up} 1
#elif MODE_{up} == 1
# define STEP_{up} 2
#else
# define STEP_{up} 3
#endif
#ifndef UNUSED_{up}
# define UNUSED_{up} 0
#endif

#ifdef STEP_{up}
int walk_{noun}(int count) {{
    int total = UNUSED_{up};

    while (count-- > 0)
        total = total + STEP_{up};
#if defined(MODE_{up}) && MODE_{up} > 1
    total = total + 1;
#endif

    return total;
}}
#else
int walk_{noun}(int count) {{ return -count; }}
#endif
""")
            calls.append(f"    total = total + walk_{noun}({k + 2});")
        elif kind == 2:
            parts.append(f"""#define FIELD_{up}(name) field_{noun}_##name
#define TEXT_{up}(x) #x

struct holder_{noun} {{
    int FIELD_{up}(low);
    int FIELD_{up}(high);
}};

int paste_{noun}(int base) {{
    struct holder_{noun} held;

    held.FIELD_{up}(low) = base;
    held.FIELD_{up}(high) = base + {k};

    return held.FIELD_{up}(high) - held.FIELD_{up}(low)
           + (int) sizeof TEXT_{up}(base + 1);
}}
""")
            calls.append(f"    total = total + paste_{noun}({k});")
        elif kind == 3:
            parts.append(f"""#define SWAP_{up}(a, b) do {{ \\
        int swapped_ = (a); \\
        (a) = (b); \\
        (b) = swapped_; \\
    }} while (0)
#define FIRST_{up}(first, ...) (first)
#define COUNT_{up}(...) (sizeof((int[]){{ __VA_ARGS__ }}) / sizeof(int))

int order_{noun}(int low, int high) {{
    if (low > high)
        SWAP_{up}(low, high);

    return high - low + FIRST_{up}({k}, 9, 8) + (int) COUNT_{up}(1, 2, 3);
}}
""")
            calls.append(f"    total = total + order_{noun}({k + 5}, {k});")
        elif kind == 4:
            parts.append(f"""#define ADD_{up}(a, b) ((a) + (b))
#define TWICE_{up}(a) ADD_{up}(a, a)
#define QUAD_{up}(a) TWICE_{up}(TWICE_{up}(a))

int nest_{noun}(int value) {{
    int result = QUAD_{up}(value) - ADD_{up}(value, {k});

#undef ADD_{up}
#define ADD_{up}(a, b) ((a) - (b))
    return result + ADD_{up}(value, 1);
}}
""")
            calls.append(f"    total = total + nest_{noun}({k + 1});")
        else:
            parts.append(f"""#define ODD_{up}(v) ((v) & 1)
#define PICK_{up}(v) (ODD_{up}(v) ? (v) * 3 + 1 : (v) / 2)
#if (PICK_{up}(6) == 3) && !defined(NOWHERE_{up})
# define STEPS_{up} 4
#else
# define STEPS_{up} 0
#endif

int collatz_{noun}(int value) {{
    int steps = 0;

    while (value != 1 && steps < STEPS_{up} * 10) {{
        value = PICK_{up}(value);
        steps++;
    }}

    return steps + __LINE__ % 3;
}}
""")
            calls.append(f"    total = total + collatz_{noun}({k + 6});")

    main_head = """int main(void) {
    int total = 0;
"""
    return parts, main_head, calls, "total"


def declare(fn_count):
    """C99's ways to declare and initialise: designators for members and
    elements, compound literals, arrays whose length the program works out
    -- local and as parameters -- declarations after statements and in a
    for, _Bool, inline, restrict, auto, an enum's own values and a struct
    that ends in an array with no size."""
    parts = []
    calls = []
    for i, noun in enumerate(names(fn_count)):
        k = i % 7
        kind = i % 6
        up = noun.upper() + str(i)
        if kind == 0:
            parts.append(f"""struct point_{noun} {{ int x, y, z; }};

static const int table_{noun}[8] = {{ [2] = {k}, [5] = 7, [7] = 1 }};

int design_{noun}(int base) {{
    struct point_{noun} point = {{ .z = base, .x = {k} }};
    struct point_{noun} list[3] = {{ [1] = {{ .y = 4 }}, [0].x = 2 }};

    return point.x + point.y + point.z + list[1].y + list[0].x
           + table_{noun}[2] + table_{noun}[5];
}}
""")
            calls.append(f"    total = total + design_{noun}({k});")
        elif kind == 1:
            parts.append(f"""struct pair_{noun} {{ int first, second; }};

static int sum_{noun}(const struct pair_{noun} *pair) {{
    return pair->first + pair->second;
}}

int literal_{noun}(int base) {{
    int *row = (int[]){{ base, base + 1, base + {k} }};
    struct pair_{noun} copy = (struct pair_{noun}){{ .second = row[2], .first = 3 }};

    return sum_{noun}(&(struct pair_{noun}){{ base, 2 }}) + copy.first
           + copy.second + row[1];
}}
""")
            calls.append(f"    total = total + literal_{noun}({k});")
        elif kind == 2:
            parts.append(f"""int grid_{noun}(int size) {{
    int grid[size][size + 1];
    int total = 0;

    for (int row = 0; row < size; row++)
        for (int column = 0; column <= size; column++)
            grid[row][column] = row + column;
    for (int row = 0; row < size; row++)
        total += grid[row][size - row];

    return total + (int) (sizeof grid / sizeof grid[0][0]);
}}
""")
            calls.append(f"    total = total + grid_{noun}({k % 4 + 2});")
        elif kind == 3:
            parts.append(f"""static void fill_{noun}(int rows, int columns, int cells[rows][columns],
                   int *restrict last) {{
    for (int row = 0; row < rows; row++)
        for (int column = 0; column < columns; column++)
            cells[row][column] = row * columns + column;
    *last = cells[rows - 1][columns - 1];
}}

int shape_{noun}(int rows) {{
    int cells[rows][3];
    int last = 0;

    fill_{noun}(rows, 3, cells, &last);

    return last + cells[0][1];
}}
""")
            calls.append(f"    total = total + shape_{noun}({k % 3 + 2});")
        elif kind == 4:
            parts.append(f"""static inline _Bool even_{noun}(int value) {{ return !(value & 1); }}

int count_{noun}(int limit) {{
    auto int seen = 0;

    for (int value = 0; value < limit; value++) {{
        _Bool even = even_{noun}(value);

        if (!even)
            continue;
        seen++;
        int bonus = value > {k} ? 1 : 0;
        seen += bonus;
    }}
    const _Bool many = seen > 2;

    return seen + many;
}}
""")
            calls.append(f"    total = total + count_{noun}({k + 3});")
        else:
            parts.append(f"""enum level_{noun} {{ LOW_{up} = 1, MIDDLE_{up} = 4, HIGH_{up} = MIDDLE_{up} * 2 }};

struct packet_{noun} {{
    int size;
    unsigned char data[];
}};

int grade_{noun}(int value) {{
    static union {{
        struct packet_{noun} packet;
        unsigned char bytes[sizeof(struct packet_{noun}) + 4];
    }} storage;
    struct packet_{noun} *packet = &storage.packet;

    packet->size = 4;
    for (int i = 0; i < packet->size; i++)
        packet->data[i] = (unsigned char) (value + i);
    enum level_{noun} level = value > 5 ? HIGH_{up} : value > 2 ? MIDDLE_{up} : LOW_{up};
    switch (level) {{
    case LOW_{up}:
        return packet->data[0];
    case MIDDLE_{up}:
        return packet->data[1] + 1;
    default:
        return packet->data[3] + HIGH_{up};
    }}
}}
""")
            calls.append(f"    total = total + grade_{noun}({k});")

    main_head = """int main(void) {
    int total = 0;
"""
    return parts, main_head, calls, "total"


def numeric(fn_count):
    """long long and unsigned long long multiplied, divided, shifted and
    compared; float and double added, multiplied, divided and compared; and
    conversions among them and int. Every floating value is one a four-byte
    float holds exactly -- the Agon's double is its float, and the host's is
    not -- so the host's answer is the Agon's."""
    parts = []
    calls = []
    for i, noun in enumerate(names(fn_count)):
        k = i % 7
        kind = i % 6
        if kind == 0:
            parts.append(f"""long long product_{noun}(long long left, long long right) {{
    long long product = left * right;

    return product / 7 + product % 7 - (left << 3) + (right >> 2);
}}
""")
            calls.append(f"    total = total + (int) (product_{noun}({k + 1}23456LL, "
                         f"{k + 2}001LL) % 1000);")
        elif kind == 1:
            parts.append(f"""unsigned long long mix_{noun}(unsigned long long seed, int rounds) {{
    for (int round = 0; round < rounds; round++) {{
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
    }}

    return seed > 0xffffffffULL ? seed >> 32 : seed;
}}
""")
            calls.append(f"    total = total + (int) (mix_{noun}({k + 1}ULL, 3) % 997);")
        elif kind == 2:
            parts.append(f"""float blend_{noun}(float left, float right, int steps) {{
    float sum = 0.0f;

    for (int step = 0; step < steps; step++)
        sum = sum + left * 0.5f - right / 4.0f;

    return sum;
}}
""")
            calls.append(f"    total = total + (int) (blend_{noun}({k + 2}, 2.0f, "
                         f"{k + 1}) * 2.0f);")
        elif kind == 3:
            parts.append(f"""double ratio_{noun}(int numerator, int denominator) {{
    double ratio = (double) numerator / denominator;

    if (ratio > 1.5)
        ratio -= 1.0;
    else if (ratio <= 0.25)
        ratio = ratio * 4.0;

    return ratio * 8.0;
}}
""")
            calls.append(f"    total = total + (int) ratio_{noun}({k + 3}, {1 << (k % 4)});")
        elif kind == 4:
            parts.append(f"""int convert_{noun}(int value) {{
    float scaled = (float) value * 1.5f;
    long long wide = (long long) scaled * 1000LL;
    unsigned char low = (unsigned char) (wide % 256);
    double half = (double) low / 2.0;

    return (int) half + (scaled > 10.0f) + (value < 0 ? -1 : 1);
}}
""")
            calls.append(f"    total = total + convert_{noun}({k - 2});")
        else:
            parts.append(f"""long long squares_{noun}(const int *values, int count) {{
    long long sum = 0;

    for (int i = 0; i < count; i++)
        sum += (long long) values[i] * values[i];

    return sum > {k}000LL ? sum - {k}000LL : sum;
}}
""")
            calls.append(f"    total = total + (int) squares_{noun}((int[]){{ {k}, {k + 1}, "
                         f"{k + 2}, 100, 200 }}, 5);")

    main_head = """int main(void) {
    int total = 0;
"""
    return parts, main_head, calls, "total"


HEADERS = {
    "pointers.c": """/* The pointer benchmark input.
 *
 * Pointers to every width, read and written through, a pointer to a
 * pointer, a pointer walked along an array and two subtracted: what acc
 * gained with pointers and none of the older inputs uses.
 *
 * Generated by generate.py and committed, so that a number from today can
 * be compared with one from last week. Returns 42.
 */
""",
    "operators.c": """/* The operator benchmark input.
 *
 * && || and ! with their short circuits, ?: nested, ++ and -- on both
 * sides, every compound assignment, and hex and octal constants: the
 * operators acc gained after the older inputs were written, which none of
 * them uses.
 *
 * Generated by generate.py and committed, so that a number from today can
 * be compared with one from last week. Returns 42.
 */
""",
    "jumps.c": """/* The jump benchmark input.
 *
 * switch -- dense with fall-through and default, sparse over a long, inside
 * a loop -- do-while, break and continue in every loop, and Duff's device:
 * the statements that leave or re-enter the middle of a construct, which
 * the older inputs have none of.
 *
 * Generated by generate.py and committed, so that a number from today can
 * be compared with one from last week. Returns 42.
 */
""",
    "matrix.c": """/* The matrix benchmark input.
 *
 * Arrays of arrays, local and global, in two and three dimensions and
 * initialised with nested braces and without; pointers to whole arrays,
 * stepped and dereferenced and taken with &; void functions that fill and
 * rotate them; and goto, leaving two loops at once. What acc gained after
 * the other inputs were written.
 *
 * Generated by generate.py and committed, so that a number from today can
 * be compared with one from last week. Returns 42.
 */
""",
    "text.c": """/* The text benchmark input.
 *
 * Character constants and their escapes, string literals as arguments and
 * as the initial values of char arrays, scanning and comparing through char
 * pointers, a table of words in a 2-D char array, and declarations inside
 * nested blocks: what acc gained after the other inputs were written.
 *
 * Generated by generate.py and committed, so that a number from today can
 * be compared with one from last week. Returns 42.
 */
""",
    "records.c": """/* The records benchmark input.
 *
 * Structs and unions -- nested, anonymous, in arrays, initialised at file
 * scope, reached through pointers and passed and returned by value -- with
 * enums as their tags and switch labels, typedefs for them and for scalars,
 * casts and sizeof: what acc gained after the other inputs were written.
 *
 * Generated by generate.py and committed, so that a number from today can
 * be compared with one from last week. Returns 42.
 */
""",
    "linkage.c": """/* The linkage benchmark input.
 *
 * Prototypes for every function, the calls to which come before the
 * definitions; static functions and static locals kept between calls;
 * const parameters, locals, pointers and tables; and variables declared
 * extern before they are defined, and again in a block: what acc gained
 * after the other inputs were written.
 *
 * Generated by generate.py and committed, so that a number from today can
 * be compared with one from last week. Returns 42.
 */
""",
    "features.c": """/* The benchmark input for what the compiler gained last: bit-fields packed
 * and read back, pointers to functions in a table and called through,
 * variable arguments with va_list, long long and unsigned long long
 * arithmetic, _Bool, the qualifiers const, volatile and register, and a
 * struct on both sides of ?:. None of the other inputs has any of it, so
 * the whole of those paths went in unmeasured.
 *
 * Generated by generate.py and committed, so that a number from today can
 * be compared with one from last week. Returns 42.
 */
""",
    "preproc.c": """/* The preprocessor benchmark input.
 *
 * Macros defined as each function needs them: constants, function-like
 * ones nested three deep, # and ##, one with __VA_ARGS__, one spread over
 * lines with backslashes, one taken back with #undef and given again; and
 * #if, #elif, #else, #ifdef, #ifndef and defined() over them. A real C file
 * is full of these, and no generated input had one: the preprocessor's
 * time went in unmeasured.
 *
 * Generated by generate.py and committed, so that a number from today can
 * be compared with one from last week. Returns 42.
 */
""",
    "declare.c": """/* The C99 declarations benchmark input.
 *
 * Designated initialisers for members and elements, compound literals,
 * arrays whose length the program works out -- local, and as parameters
 * with restrict beside them -- declarations after statements and in for,
 * _Bool, inline, auto, an enum with values of its own, and a struct that
 * ends in an array with no size. What C99 added to declaring things, which
 * the older inputs have little or none of.
 *
 * Generated by generate.py and committed, so that a number from today can
 * be compared with one from last week. Returns 42.
 */
""",
    "numeric.c": """/* The numeric benchmark input.
 *
 * long long and unsigned long long multiplied, divided, shifted and
 * compared; float and double added, multiplied, divided and compared; and
 * conversions among them, int and unsigned char. wide.c only passes floats
 * around, from before acc had their arithmetic, so none of it was
 * measured. Every floating value is one a four-byte float holds exactly,
 * so the host's answer is the Agon's.
 *
 * Generated by generate.py and committed, so that a number from today can
 * be compared with one from last week. Returns 42.
 */
""",
    "data.c": """/* The data benchmark input.
 *
 * Globals and global arrays declared between the functions that use them,
 * with and without initial values; local arrays, one of them initialised in
 * part; and for loops, with a declaration in the first clause and without:
 * what acc gained after the older inputs were written, which none of them
 * uses.
 *
 * Generated by generate.py and committed, so that a number from today can
 * be compared with one from last week. Returns 42.
 */
""",
}


def assemble(header, parts, main_head, calls, var, subtract, after=False):
    main = main_head + "\n".join(calls) + f"\n\n    return {var} - {subtract};\n}}\n"
    if after:       # the functions after main, called before they are defined
        return header + "\n" + main + "\n" + "\n".join(parts)
    return header + "\n" + "\n".join(parts) + "\n" + main


def host_result(source):
    """What main returns, compiled by the host's compiler: a char is signed
    on the Agon, so it is here too."""
    with tempfile.TemporaryDirectory() as tmp:
        c = os.path.join(tmp, "t.c")
        exe = os.path.join(tmp, "t")
        with open(c, "w") as f:
            # acc has va_list as a keyword and no preprocessor, so the input
            # says `va_list` with nothing included; the host needs the
            # header, which goes in here and not in what is written out.
            # And #line, so that __LINE__ -- which preproc.c reads -- counts
            # from the input's first line and not from the header's.
            f.write("#include <stdarg.h>\n#line 1\n"
                    + source.replace("int main(void)", "int bench_main(void)")
                    + '\n#include <stdio.h>\nint main(void) '
                      '{ printf("%d\\n", bench_main()); return 0; }\n')
        subprocess.run(["cc", "-std=c99", "-fsigned-char", "-w", "-o", exe, c],
                       check=True)
        return int(subprocess.run([exe], check=True, capture_output=True,
                                  text=True).stdout)


def write(name, build, target_bytes=17500, after=False):
    fn_count = 10
    while True:
        parts, head, calls, var = build(fn_count)
        source = assemble(HEADERS[name], parts, head, calls, var, 0, after)
        if len(source) >= target_bytes:
            break
        fn_count += 1

    total = host_result(source)
    if abs(total) > 1000000:
        sys.exit(f"{name}: the total, {total}, is too near a 24-bit int's edge")
    source = assemble(HEADERS[name], parts, head, calls, var, total - 42, after)
    assert host_result(source) == 42
    with open(os.path.join(HERE, name), "w") as f:
        f.write(source)
    print(f"{name}: {fn_count} functions, {len(source)} bytes")


if __name__ == "__main__":
    write("pointers.c", pointers)
    write("operators.c", operators)
    write("data.c", data)
    write("jumps.c", jumps)
    write("matrix.c", matrix)
    write("text.c", text)
    write("records.c", records)
    write("linkage.c", linkage, after=True)
    write("features.c", features)
    write("preproc.c", preproc)
    write("declare.c", declare)
    write("numeric.c", numeric)
