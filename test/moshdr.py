#!/usr/bin/env python3
"""Wrap raw eZ80 code in a MOS program header.

The layout, read off a working agondev binary rather than from documentation:

    0x00  c3 45 00 04     jp $040045, over the header
    0x04  program name, NUL padded
    0x40  "MOS"
    0x43  header version (0)
    0x44  1 = ADL mode, 24-bit addressing
    0x45  code, loaded and entered at 0x040045

acc has to emit exactly this, so the one definition lives here and the test
helpers are built from it -- if it is wrong, the helpers stop working and the
whole Agon suite says so, rather than the error waiting until acc emits it.
"""
import sys

HEADER_SIZE = 0x45
LOAD_ADDR = 0x040000


def wrap(name: str, code: bytes) -> bytes:
    b = bytearray(b"\xc3\x45\x00\x04")
    n = name.encode()[:59]
    b += n + b"\x00" * (0x40 - 4 - len(n))
    b += b"MOS" + bytes([0, 1])
    assert len(b) == HEADER_SIZE, hex(len(b))
    return bytes(b) + code


if __name__ == "__main__":
    # test/moshdr.py <name> <out.bin> <hex bytes...>
    name, out = sys.argv[1], sys.argv[2]
    code = bytes(int(x, 16) for x in sys.argv[3:])
    with open(out, "wb") as f:
        f.write(wrap(name, code))
