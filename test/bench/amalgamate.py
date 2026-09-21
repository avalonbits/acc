#!/usr/bin/env python3
"""One translation unit of a real program, with its includes folded in.

    test/bench/amalgamate.py <source dir> <file.c> [include dir ...]

The benchmark's other inputs are generated: the same shapes over and over at
the sizes and name lengths real C has. What they cannot be is a real
program's proportions -- how much of it is declarations against statements,
how deep the expressions go, how many of the calls are to things declared in
a header that this file happens not to use. zap is the program acc exists to
build on the machine, so zap is what that is taken from.

It is folded into one file because the Agon the benchmark runs on has one
file on its card: there is no -I to give and no header to find. Each file is
folded in once, which is what its include guard would have done anyway, and
what is left after that is exactly the bytes acc reads -- so the cycles per
byte are over the same bytes the figure names.
"""
import os
import re
import sys

INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^">]+)[">]')


def fold(path, dirs, seen, out):
    if path in seen:
        return
    seen.add(path)
    here = os.path.dirname(path)
    with open(path) as f:
        for line in f:
            m = INCLUDE.match(line)
            if not m:
                out.append(line)
                continue
            name = m.group(2)
            where = [here] + dirs if m.group(1) == '"' else dirs
            for d in where:
                full = os.path.join(d, name)
                if os.path.exists(full):
                    fold(full, dirs, seen, out)
                    break
            else:
                sys.exit('%s: cannot find %s' % (path, name))


def main(argv):
    if len(argv) < 3:
        sys.exit(__doc__)
    src, top, dirs = argv[1], argv[2], argv[3:]
    out = []
    fold(os.path.join(src, top), dirs, set(), out)
    sys.stdout.write(''.join(out))


if __name__ == '__main__':
    main(sys.argv)
