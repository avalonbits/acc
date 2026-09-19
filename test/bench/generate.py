#!/usr/bin/env python3
"""Writes the benchmark inputs that cover what the older ones do not.

    test/bench/generate.py            # rewrites pointers.c, operators.c,
                                      # data.c, jumps.c, matrix.c, text.c,
                                      # records.c and linkage.c

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
            f.write(source.replace("int main(void)", "int bench_main(void)")
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
