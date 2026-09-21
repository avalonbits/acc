#!/usr/bin/env python3
"""That a program clears what it says starts at zero.

Read out of the image rather than watched happening, because watching it
needs memory that is dirty first, and the only way to dirty it is to run
another program -- which means returning to MOS, which an acc-compiled
program does not yet survive.

What is checked is the whole of the arrangement: the program calls a routine
before it calls anything else, that routine clears from the image's last byte
on, and it clears exactly as many bytes as the variables that wanted them --
no more, so that the table the argument routine keeps past the end of it is
not being zeroed for nothing."""
import sys

LOAD = 0x040000
ENTRY = 0x45

# The call to the clearing is not the first instruction: the stub saves what
# MOS wants back and the command line it was given before anything else runs.
CLEAR = ENTRY + 3


def u24(b, at):
    return b[at] | (b[at + 1] << 8) | (b[at + 2] << 16)


def main(path, bss_bytes):
    b = open(path, 'rb').read()

    if b[CLEAR] != 0xcd:
        return 'the program does not call the clearing (%02x)' % b[CLEAR]
    at = u24(b, CLEAR + 1) - LOAD
    if not 0 <= at < len(b):
        return 'it calls %06x, which is outside the image' % (at + LOAD)

    if not bss_bytes:
        if b[at] != 0xc9:
            return 'nothing starts at zero, so it should call a ret, not %02x' % b[at]
        return None

    # One byte needs no ldir: `ld (hl), 0` has already cleared it, and an
    # ldir of what is left would be one of bc = 0, which this chip reads as
    # sixteen megabytes rather than as nothing.
    if bss_bytes == 1:
        want = [0x21, None, None, None, 0x36, 0x00, 0xc9]
    else:
        want = [0x21, None, None, None, 0x36, 0x00, 0x11, None, None, None,
                0x01, None, None, None, 0xed, 0xb0, 0xc9]
    if at + len(want) > len(b):
        return 'the clearing runs off the end of the image'
    for i, byte in enumerate(want):
        if byte is not None and b[at + i] != byte:
            return ('the clearing is not what it should be: byte %d is %02x '
                    'and should be %02x' % (i, b[at + i], byte))

    base = u24(b, at + 1)
    if base != LOAD + len(b):
        return ('it clears from %06x, and the image ends at %06x'
                % (base, LOAD + len(b)))
    if bss_bytes == 1:
        return None
    if u24(b, at + 7) != base + 1:
        return 'the second address is not one past the first'
    if u24(b, at + 11) != bss_bytes - 1:
        return ('it clears %d bytes and %d start at zero'
                % (u24(b, at + 11) + 1, bss_bytes))

    return None


if __name__ == '__main__':
    why = main(sys.argv[1], int(sys.argv[2]))
    if why:
        print(why)
        sys.exit(1)
