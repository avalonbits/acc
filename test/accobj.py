#!/usr/bin/env python3
# A writer of ACC version 1 objects, from the format as src/obj.c describes
# it and not from acc's own writer: what an assembler that writes them -- zap
# -- is held to, and what the linker's handling of the kinds acc itself never
# writes is tested with. See test/accobj.sh.
#
#   obj = Obj()
#   obj.item(0, align=8)                       # an item, at an offset
#   obj.text += bytes([...])
#   obj.define('_f', 4, func=True)             # a C name has its underscore
#   obj.reloc(9, 'LOW8', '_f')                 # the addend is in the slot
#   obj.reloc(12, 'HIGH8', '_f', addend=0x1ff) # an addend of its own
#   obj.write('f.o')
#
# Targets are 'text', 'bss' or a symbol's name; a name the object does not
# define is added to it, undefined. The writer puts the symbols in order of
# their names and the relocations in order of their slots, as zap does, and
# `raw` lets a test break the rules on purpose.

import sys

KINDS = {'ABS24': 0, 'LOW8': 1, 'HIGH8': 2, 'UPPER8': 3, 'PCREL8': 4, 'ABS16': 5}
DEFINED, FUNC, BSS = 1, 2, 4


def n3(value):
    return (value & 0xffffff).to_bytes(3, 'little')


class Obj:
    def __init__(self):
        self.text = bytearray()
        self.bss_len = 0
        self.bss_align = 0
        self.syms = []          # (name, value, flags)
        self.relocs = []        # (at, target, kind, addend or None)
        self.items = []         # (offset, align)
        self.raw = None         # replaces the relocation tables when set

    def item(self, offset, align=0):
        self.items.append((offset, align))

    def define(self, name, value, func=False, bss=False):
        self.syms.append((name, value, DEFINED | (FUNC if func else 0)
                          | (BSS if bss else 0)))

    def reloc(self, at, kind, target, addend=None):
        self.relocs.append((at, target, KINDS[kind] if isinstance(kind, str)
                            else kind, addend))

    def _index(self, target):
        if target == 'text':
            return 0
        if target == 'bss':
            return 1
        for i, (name, _, _) in enumerate(self.syms):
            if name == target:
                return i + 2
        raise ValueError(target)

    def write(self, path):
        strings = bytearray()
        offsets = []
        rel, rel_a = [], []

        # Symbols sorted by name, bytewise, defined and undefined together --
        # which is how zap writes them, so that the two writers' bytes can be
        # compared: a relocation names its target by position.
        names = {name for name, _, _ in self.syms}
        for _, target, _, _ in self.relocs:
            if target not in ('text', 'bss') and target not in names:
                self.syms.append((target, 0, 0))
                names.add(target)
        self.syms.sort(key=lambda sym: sym[0].encode())

        for at, target, kind, addend in sorted(self.relocs, key=lambda r: r[0]):
            tk = self._index(target) | kind << 20
            if addend is None:
                rel.append(n3(at) + n3(tk))
            else:
                rel_a.append(n3(at) + n3(tk) + n3(addend))
        if self.raw is not None:
            rel, rel_a = self.raw
        for name, _, _ in self.syms:
            offsets.append(len(strings))
            strings += name.encode() + b'\0'
        out = bytearray(b'ACC\x01')
        out += n3(0)                                    # build: not acc's
        out += n3(len(self.text))
        out += n3(self.bss_len | self.bss_align << 20)
        out += n3(len(self.syms))
        out += n3(len(rel))
        out += n3(0)                                    # ndeps
        out += n3(len(strings))
        out += n3(len(self.items))
        out += n3(len(rel_a))
        for (name, value, flags), at in zip(self.syms, offsets):
            out += n3(at) + n3(value) + bytes([flags])
        for r in rel:
            out += r
        for r in rel_a:
            out += r
        for offset, align in self.items:
            out += n3(offset | align << 20)
        out += strings
        out += self.text
        with open(path, 'wb') as f:
            f.write(out)


if __name__ == '__main__':
    sys.exit('accobj.py is a module: see test/accobj.sh')
