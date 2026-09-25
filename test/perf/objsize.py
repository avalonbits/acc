#!/usr/bin/env python3
# The size of each function in an object, as acc and agondev built it.
#
#   objsize.py acc <object.o>...      acc's objects, the format of src/obj.c
#   objsize.py elf <nm> <object.o>... agondev's, through its nm
#   objsize.py acc-text <object.o>... the text of acc's objects, summed
#
# Prints `name size`, a line per function and per variable with room in the
# text. An acc object says where each function and variable begins -- its
# items, the text cut at each -- and each runs to where the next begins; an
# ELF object says its symbols' sizes itself. The names are C's, without the
# underscore in front both compilers give them.

import subprocess
import sys


def n3(data, at):
    return data[at] | data[at + 1] << 8 | data[at + 2] << 16


def acc_sizes(path):
    data = open(path, 'rb').read()
    if data[:4] != b'ACC\x01':
        sys.exit('%s: not an ACC version 1 object' % path)
    text_len = n3(data, 7)
    nsyms, nrelocs, ndeps = n3(data, 13), n3(data, 16), n3(data, 19)
    strings_len, nitems, nrelocs_a = n3(data, 22), n3(data, 25), n3(data, 28)
    at = 31
    syms = []
    for i in range(nsyms):
        syms.append((n3(data, at), n3(data, at + 3), data[at + 6]))
        at += 7
    at += nrelocs * 6 + nrelocs_a * 9 + ndeps * 12
    items = sorted(n3(data, at + 3 * i) & 0xfffff for i in range(nitems))
    at += nitems * 3
    strings = data[at:at + strings_len]
    ends = items[1:] + [text_len]
    names = {}
    for name_at, value, flags in syms:
        # Defined, with room in the text rather than the bss.
        if flags & 1 and not flags & 4:
            name = strings[name_at:strings.index(b'\0', name_at)].decode()
            names[value] = name.lstrip('_')
    # A `static` function or variable has an item and no symbol: it is
    # named by where it comes in the file, which is the order it was
    # written in.
    out = []
    for n, (start, end) in enumerate(zip(items, ends)):
        if end > start:
            out.append((names.get(start, '(static %d)' % n), end - start))
    return out


def elf_sizes(nm, path):
    out = []
    listing = subprocess.run([nm, '-S', path], capture_output=True, text=True,
                             check=True).stdout
    for line in listing.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[2] in 'TtDdRr':
            out.append((parts[3].lstrip('_'), int(parts[1], 16)))
    return out


def acc_text(path):
    data = open(path, 'rb').read()

    return n3(data, 7)


def main():
    if sys.argv[1] == 'acc-text':       # the objects' text, all told
        print(sum(acc_text(p) for p in sys.argv[2:]))
        return
    if sys.argv[1] == 'acc':
        for path in sys.argv[2:]:
            for name, size in acc_sizes(path):
                print(name, size)
    else:
        for path in sys.argv[3:]:
            for name, size in elf_sizes(sys.argv[2], path):
                print(name, size)


if __name__ == '__main__':
    main()
