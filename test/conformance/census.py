#!/usr/bin/env python3
# What a test is made of: the C99 constructs in it, each named after the
# clause of the standard it belongs to. Taken for every test at import,
# whether or not it can run, so that a test excluded or waiting on a
# feature still says what it was the cover for. See the plan's section
# "When a test cannot run, keep what it was testing".
#
#   census.py <file.c>...       one line per file: name<TAB>constructs
#
# Mechanical, and only good enough to point at the right clause: a regular
# tokenizer over the text with its comments taken out, then tests on the
# tokens. A construct's name is `<clause>:<what>`, and the clause is the
# part before the colon.

import re
import sys

# The C99 library, by the clause of the header that declares it.
HEADERS = {
    'assert': '7.2', 'complex': '7.3', 'ctype': '7.4', 'errno': '7.5',
    'fenv': '7.6', 'float': '7.7', 'inttypes': '7.8', 'iso646': '7.9',
    'limits': '7.10', 'locale': '7.11', 'math': '7.12', 'setjmp': '7.13',
    'signal': '7.14', 'stdarg': '7.15', 'stdbool': '7.16', 'stddef': '7.17',
    'stdint': '7.18', 'stdio': '7.19', 'stdlib': '7.20', 'string': '7.21',
    'tgmath': '7.22', 'time': '7.23', 'wchar': '7.24', 'wctype': '7.25',
}
LIBRARY = {
    '7.2 assert': 'assert',
    '7.4 ctype': 'isalnum isalpha isblank iscntrl isdigit isgraph islower '
                 'isprint ispunct isspace isupper isxdigit tolower toupper',
    '7.12 math': 'acos asin atan atan2 cos sin tan acosh asinh atanh cosh sinh '
                 'tanh exp exp2 expm1 frexp ilogb ldexp log log10 log1p log2 '
                 'logb modf scalbn scalbln cbrt fabs hypot pow sqrt erf erfc '
                 'lgamma tgamma ceil floor nearbyint rint lrint llrint round '
                 'lround llround trunc fmod remainder remquo copysign nan '
                 'nextafter fdim fmax fmin fma isnan isinf isfinite signbit '
                 'fpclassify isnormal isgreater isless '
                 'sqrtf fabsf floorf ceilf',
    '7.13 setjmp': 'setjmp longjmp',
    '7.14 signal': 'signal raise',
    '7.15 stdarg': 'va_start va_arg va_end va_copy',
    '7.19 stdio': 'printf fprintf sprintf snprintf vprintf vfprintf vsprintf '
                  'vsnprintf scanf fscanf sscanf vsscanf puts fputs putchar '
                  'fputc putc getchar fgetc getc fgets ungetc fopen fclose '
                  'fread fwrite fseek ftell rewind fflush remove rename tmpfile',
    '7.20 stdlib': 'malloc calloc realloc free abort exit atexit _Exit atoi atol '
                   'atoll strtol strtoul strtoll strtoull strtod strtof abs labs '
                   'llabs div ldiv lldiv qsort bsearch rand srand getenv system',
    '7.21 string': 'memcpy memmove memcmp memchr memset strcpy strncpy strcat '
                   'strncat strcmp strncmp strcoll strxfrm strchr strrchr strspn '
                   'strcspn strpbrk strstr strtok strlen strerror',
    '7.23 time': 'time clock difftime mktime asctime ctime gmtime localtime '
                 'strftime',
    '7.24 wchar': 'wcslen wcscpy wcscmp wprintf swprintf mbstowcs wcstombs',
}
LIBRARY = {name: clause.split()[0] for clause, names in LIBRARY.items()
           for name in names.split()}

KEYWORDS = {
    'auto': '6.7.1:auto', 'register': '6.7.1:register',
    'static': '6.7.1:static', 'extern': '6.7.1:extern',
    'typedef': '6.7.7:typedef',
    'char': '6.7.2:char', 'short': '6.7.2:short', 'int': '6.7.2:int',
    'long': '6.7.2:long', 'float': '6.7.2:float', 'double': '6.7.2:double',
    'signed': '6.7.2:signed', 'unsigned': '6.7.2:unsigned',
    'void': '6.7.2:void', '_Bool': '6.7.2:_Bool',
    '_Complex': '6.7.2:_Complex',
    'struct': '6.7.2.1:struct', 'union': '6.7.2.1:union',
    'enum': '6.7.2.2:enum',
    'const': '6.7.3:const', 'volatile': '6.7.3:volatile',
    'restrict': '6.7.3:restrict', 'inline': '6.7.4:inline',
    'if': '6.8.4.1:if', 'else': '6.8.4.1:else', 'switch': '6.8.4.2:switch',
    'case': '6.8.1:case', 'default': '6.8.1:default',
    'while': '6.8.5.1:while', 'do': '6.8.5.2:do', 'for': '6.8.5.3:for',
    'goto': '6.8.6.1:goto', 'continue': '6.8.6.2:continue',
    'break': '6.8.6.3:break', 'return': '6.8.6.4:return',
    'sizeof': '6.5.3.4:sizeof',
}

OPERATORS = {
    '[': '6.5.2.1:subscript', '->': '6.5.2.3:arrow', '.': '6.5.2.3:member',
    '++': '6.5.2.4:increment', '--': '6.5.2.4:decrement',
    '~': '6.5.3.3:complement', '!': '6.5.3.3:not',
    '*': '6.5.5:multiply', '/': '6.5.5:divide', '%': '6.5.5:remainder',
    '+': '6.5.6:add', '-': '6.5.6:subtract',
    '<<': '6.5.7:shift', '>>': '6.5.7:shift',
    '<': '6.5.8:relational', '>': '6.5.8:relational',
    '<=': '6.5.8:relational', '>=': '6.5.8:relational',
    '==': '6.5.9:equality', '!=': '6.5.9:equality',
    '&': '6.5.10:and', '^': '6.5.11:xor', '|': '6.5.12:or',
    '&&': '6.5.13:logical-and', '||': '6.5.14:logical-or',
    '?': '6.5.15:conditional',
    '=': '6.5.16.1:assign',
    '*=': '6.5.16.2:compound-assign', '/=': '6.5.16.2:compound-assign',
    '%=': '6.5.16.2:compound-assign', '+=': '6.5.16.2:compound-assign',
    '-=': '6.5.16.2:compound-assign', '<<=': '6.5.16.2:compound-assign',
    '>>=': '6.5.16.2:compound-assign', '&=': '6.5.16.2:compound-assign',
    '^=': '6.5.16.2:compound-assign', '|=': '6.5.16.2:compound-assign',
    ',': '6.5.17:comma',
    '<:': '6.4.6:digraph', ':>': '6.4.6:digraph', '<%': '6.4.6:digraph',
    '%>': '6.4.6:digraph', '%:': '6.4.6:digraph',
}

TOKEN = re.compile(r'''
    (?P<str>L?"(?:\\.|[^"\\\n])*")
  | (?P<chr>L?'(?:\\.|[^'\\\n])*')
  | (?P<num>\.?\d(?:[eEpP][+-]|[\w.])*)
  | (?P<id>[A-Za-z_]\w*)
  | (?P<op>%:%:|<<=|>>=|\.\.\.|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||[*/%+\-&^|]=
          |<:|:>|<%|%>|%:|\#\#|[\[\](){}.&*+\-~!/%<>^|?:;=,\#])
''', re.X)


def strip_comments(text):
    """Comments out, strings and character constants left whole."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('/*', i):
            j = text.find('*/', i + 2)
            i = n if j < 0 else j + 2
            out.append(' ')
        elif text.startswith('//', i):
            j = text.find('\n', i)
            i = n if j < 0 else j
        elif c in '"\'':
            j = i + 1
            while j < n and text[j] != c and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def census(text):
    found = set()
    text = strip_comments(text).replace('\\\n', '')

    # The preprocessor, a line at a time.
    body = []
    for line in text.split('\n'):
        m = re.match(r'\s*(?:#|%:)\s*(\w+)(.*)', line)
        if not m:
            body.append(line)
            continue
        d, rest = m.group(1), m.group(2)
        if d == 'include':
            found.add('6.10.2:include')
            h = re.search(r'<(\w+)\.h>', rest)
            if h and h.group(1) in HEADERS:
                found.add('%s:<%s.h>' % (HEADERS[h.group(1)], h.group(1)))
        elif d == 'define':
            found.add('6.10.3:define-function' if re.match(r'\s*\w+\(', rest)
                      else '6.10.3:define-object')
            if '__VA_ARGS__' in rest:
                found.add('6.10.3:variadic-macro')
            if re.search(r'##|%:%:', rest):
                found.add('6.10.3.3:paste')
            elif re.search(r'(?<![#%:])#\s*\w', rest):
                found.add('6.10.3.2:stringize')
        elif d in ('if', 'ifdef', 'ifndef', 'elif', 'else', 'endif'):
            found.add('6.10.1:conditional')
        elif d == 'line':
            found.add('6.10.4:line')
        elif d == 'error':
            found.add('6.10.5:error')
        elif d == 'pragma':
            found.add('6.10.6:pragma')
        elif d == 'undef':
            found.add('6.10.3.5:undef')
        body.append('')
    toks = [(m.lastgroup, m.group()) for m in TOKEN.finditer('\n'.join(body))]
    if 'defined' in (t for _, t in toks):
        found.add('6.10.1:defined')

    depth = 0                           # braces, for what is at file scope
    for i, (kind, t) in enumerate(toks):
        nxt = toks[i + 1][1] if i + 1 < len(toks) else ''
        prev = toks[i - 1][1] if i else ''
        if kind == 'str':
            found.add('6.4.5:wide-string' if t[0] == 'L' else '6.4.5:string')
        elif kind == 'chr':
            found.add('6.4.4.4:wide-char' if t[0] == 'L' else '6.4.4.4:char')
        elif kind == 'num':
            low = t.lower()
            if low.startswith('0x') and 'p' in low:
                found.add('6.4.4.2:hex-float')
            elif re.search(r'[.e]', low) and not low.startswith('0x'):
                found.add('6.4.4.2:float')
            elif low.startswith('0x'):
                found.add('6.4.4.1:hex')
            elif low.startswith('0') and len(low) > 1 and low[1].isdigit():
                found.add('6.4.4.1:octal')
            if re.search(r'll$|llu$|ull$', low):
                found.add('6.4.4.1:long-long')
        elif kind == 'id':
            if t in KEYWORDS:
                found.add(KEYWORDS[t])
                if t == 'long' and nxt == 'long':
                    found.add('6.7.2:long-long')
            elif t in ('__func__',):
                found.add('6.4.2.2:__func__')
            elif t == '_Pragma':
                found.add('6.10.9:_Pragma')
            elif t in LIBRARY and nxt == '(':
                found.add('%s:%s' % (LIBRARY[t], t))
            elif nxt == ':' and prev in (';', '{', '}') and depth > 0:
                found.add('6.8.1:label')
        elif kind == 'op':
            if t == '{':
                depth += 1
            elif t == '}':
                depth -= 1
            if t in OPERATORS:
                name = OPERATORS[t]
                # A star or an ampersand with nothing in front that a value
                # ends with is the unary one; a minus or plus likewise.
                unary = prev in ('', '(', ',', '=', '{', '[', 'return', '?',
                                 ':', ';') or prev in OPERATORS
                if t == '*' and unary:
                    name = '6.5.3.2:dereference'
                elif t == '&' and unary:
                    name = '6.5.3.2:address'
                elif t in '+-' and unary:
                    name = '6.5.3.3:unary-' + ('plus' if t == '+' else 'minus')
                elif t in ('++', '--') and nxt not in (')', ';', ',', ']'):
                    name = '6.5.3.1:prefix-step'
                found.add(name)
            if t == '...':
                found.add('6.7.5.3:variadic')
            if t == ')' and nxt == '{' and depth > 0:
                found.add('6.5.2.5:compound-literal')
            if t in ('.', '[') and prev in ('{', ','):
                found.add('6.7.8:designator')

    # Shapes that take more than a token.
    joined = ' '.join(t for _, t in toks)
    if re.search(r'\( \* \w+ \) \(', joined):
        found.add('6.7.5.3:function-pointer')
    if re.search(r'\( \* \w+ \) \[', joined):
        found.add('6.7.5.1:pointer-to-array')
    if re.search(r'\] \[', joined):
        found.add('6.7.5.2:multidimensional')
    if re.search(r'\w+ \[ \] ;', joined):
        found.add('6.7.2.1:flexible-member')
    if re.search(r': \d+ ;', joined) and 'struct' in joined:
        found.add('6.7.2.1:bit-field')
    if re.search(r'= \{', joined):
        found.add('6.7.8:braced-initializer')
    # `f (a, b) int a; char *b; {`: names in the parentheses, and their
    # declarations between the `)` and the body.
    if re.search(r'\( \w+(?: , \w+)* \) (?:[^;{}()=]+ ; )+\{', joined):
        found.add('6.9.1:old-style-definition')

    return sorted(found)


def clauses(found):
    return sorted({c.split(':')[0] for c in found},
                  key=lambda c: [int(x) if x.isdigit() else x
                                 for x in re.split(r'[.\s]', c)])


def main():
    for path in sys.argv[1:]:
        with open(path, errors='replace') as f:
            found = census(f.read())
        name = re.sub(r'\.c$', '', path.rsplit('/', 1)[-1])
        print('%s\t%s' % (name, ' '.join(found)))


if __name__ == '__main__':
    main()
