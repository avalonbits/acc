#!/usr/bin/env python3
# What gcc's DejaGnu directives say about a test in gcc.dg: what kind of
# test it is, the options that change what it means, and -- for one that
# has to be refused -- the lines its errors are on.
#
#   dg.py <file.c>...    a line per file:
#                        name<TAB>kind<TAB>flags<TAB>lines
#
# kind is run (it is compiled, run and has to return 0), compile (it has to
# compile), reject (it has to be refused, at one of `lines`), or other
# (linked, preprocessed or assembled only, which the suite does not take).
# flags are the -D, -U and -trigraphs in its dg-options, which both
# compilers are given;
# the rest of its options -- optimisation, warnings, a -std -- are gcc's
# business, and the C99 filter decides what the test means here.
#
# A dg-error applies to the line it is written on, unless it names another:
# a number, or `.-1` / `.+2` counted from its own line.

import re
import sys


def parse(path):
    with open(path, errors='replace') as f:
        lines = f.read().split('\n')
    kind, flags, errors = 'compile', [], set()
    for number, text in enumerate(lines, 1):
        for m in re.finditer(r'\{\s*dg-(do|options|additional-options)\s+([^}]*)\}', text):
            what, rest = m.group(1), m.group(2)
            if what == 'do':
                kind = rest.split()[0] if rest.split() else kind
            else:
                opts = re.search(r'"([^"]*)"', rest)
                if opts:
                    flags += [o for o in opts.group(1).split()
                              if re.match(r'-[DU]\w', o) or o == '-trigraphs']
        for m in re.finditer(r'\{\s*dg-error\b(.*)', text):
            errors.add(error_line(m.group(1), number))
    if kind not in ('run', 'compile'):
        kind = 'other'
    elif errors:
        kind = 'reject' if kind == 'compile' else 'other'
    return kind, flags, sorted(errors)


def error_line(rest, number):
    """The line a dg-error names: its own, unless its fourth argument --
    after the message, a comment and a target selector -- says otherwise."""
    args = re.findall(r'"(?:\\.|[^"\\])*"|\{[^}]*\}|[^\s{}"]+', rest)
    args = [a for a in args if a != '}']
    if len(args) >= 4:
        where = args[3]
        m = re.match(r'^\.([+-]\d+)$', where)
        if m:
            return number + int(m.group(1))
        if where.isdigit():
            return int(where)
    return number


def main():
    for path in sys.argv[1:]:
        kind, flags, errors = parse(path)
        name = re.sub(r'\.c$', '', path.rsplit('/', 1)[-1])
        print('%s\t%s\t%s\t%s' % (name, kind, ' '.join(flags),
                                  ' '.join(map(str, errors))))


if __name__ == '__main__':
    main()
