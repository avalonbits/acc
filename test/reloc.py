#!/usr/bin/env python3
"""Two builds of one program at two load addresses, and the table that is
meant to account for the difference between them.

Reports the first thing that is wrong, in the terms the compiler would have
to be fixed in: an address that moved and was not recorded, a slot recorded
that did not move, or the wrong distance."""
import sys


def u24(b, at):
    return b[at] | (b[at + 1] << 8) | (b[at + 2] << 16)


def main(path_a, path_b, path_rel, delta):
    a = open(path_a, 'rb').read()
    b = open(path_b, 'rb').read()
    if len(a) != len(b):
        return 'the two images are %d and %d bytes' % (len(a), len(b))

    offs = [int(line, 16) for line in open(path_rel) if line.strip()]

    # Where the compiler says an address is: the right one, and it moved.
    covered = bytearray(len(a))
    for off in offs:
        if off < 0 or off + 3 > len(a):
            return 'a relocation at %06x is outside a %d-byte image' % (
                off, len(a))
        moved = u24(b, off) - u24(a, off)
        if moved != delta:
            return ('the address at %06x moved by %d and not %d'
                    % (off, moved, delta))
        for i in range(3):
            covered[off + i] = 1

    # And nowhere else did anything move.
    for i in range(len(a)):
        if a[i] != b[i] and not covered[i]:
            start = max(0, i - 8)
            return ('byte %06x changed and no relocation covers it '
                    '(%s against %s)'
                    % (i, a[start:i + 4].hex(), b[start:i + 4].hex()))

    return None


if __name__ == '__main__':
    why = main(sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]))
    if why:
        print(why)
        sys.exit(1)
