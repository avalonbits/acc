#!/usr/bin/env python3
"""Turn assembled runtime helpers into a C table acc can emit.

The helpers are written as assembly and read as assembly. Nothing here is
transcribed by hand: this assembles the source, reads the symbols and the
bytes out of the object, and writes the header gen.c includes. Getting a byte
wrong in a table of opcodes is not something review catches, so the table is
not written by a person.

The routines share code -- the four signed and unsigned division entries are
one loop with four ways in -- so they are emitted as one blob with a table of
entry points, rather than one at a time. A program that uses any of them
carries all of them; at a few hundred bytes against the Agon's 448 KB that is
the cheaper trade than four copies of a division loop.

Sharing means internal calls, which means relocations, so those are extracted
too and acc applies them when it drops the blob in. Anything that is not a
call within the blob is rejected here: there is nothing else for acc to
resolve against.
"""
import re
import subprocess
import sys


def run(*args):
    return subprocess.run(args, capture_output=True, text=True, check=True).stdout


def main(asm, out, tools):
    obj = out + '.o'
    run(tools + '/ez80-none-elf-as', '-march=ez80+full', asm, '-o', obj)

    text = subprocess.run([tools + '/ez80-none-elf-objcopy', '-O', 'binary',
                           '--only-section=.text', obj, '/dev/stdout'],
                          capture_output=True, check=True).stdout

    # Internal references: each is a 24-bit address of somewhere in .text.
    fixups = []
    for line in run(tools + '/ez80-none-elf-readelf', '-r', obj).splitlines():
        m = re.match(r'^([0-9a-f]{8})\s+\S+\s+(\S+)\s+\S+\s+(\S+)\s*\+\s*(\S+)', line)
        if not m:
            continue
        at, kind, sym, addend = m.group(1), m.group(2), m.group(3), m.group(4)
        if kind != 'R_Z80_24' or sym != '.text':
            sys.exit('%s: a helper refers to %s, which acc has nothing to resolve '
                     'against' % (asm, sym))
        fixups.append((int(at, 16), int(addend, 16)))

    # Global symbols only: the entry points, in address order.
    entries = []
    for line in run(tools + '/ez80-none-elf-nm', obj).splitlines():
        parts = line.split()
        if len(parts) != 3:
            continue                    # undefined or absolute; not an entry
        addr, kind, name = parts
        if kind == 'T' and name.startswith('_acc_rt_'):
            entries.append((int(addr, 16), name[len('_acc_rt_'):]))
    entries.sort()

    with open(out, 'w') as f:
        f.write('/* Generated from %s by %s. Do not edit.\n'
                ' *\n'
                ' * Each entry is one helper, whole and position independent.\n'
                ' */\n' % (asm, __file__.split('/')[-1]))
        f.write('#ifndef ACC_RT_HELPERS_H\n#define ACC_RT_HELPERS_H\n\n')
        f.write('static const unsigned char rt_code[] = {\n')
        for i in range(0, len(text), 12):
            f.write('    ' + ' '.join('0x%02x,' % b for b in text[i:i + 12]) + '\n')
        f.write('};\n\n')
        f.write('/* Where each entry point sits within rt_code. */\n')
        f.write('enum {\n')
        for _, name in entries:
            f.write('    RT_%s,\n' % name.upper())
        f.write('    RT_COUNT\n};\n\n')
        f.write('static const short rt_entry[RT_COUNT] = {\n')
        for addr, name in entries:
            f.write('    %d,   /* %s */\n' % (addr, name))
        f.write('};\n\n')
        f.write('/* Calls from one routine to another: the address at `at` is the\n'
                ' * blob\'s base plus `to`. */\n')
        f.write('typedef struct { short at, to; } RtFix;\n\n')
        f.write('#define RT_NFIX %d\n\n' % len(fixups))
        if fixups:
            f.write('static const RtFix rt_fix[RT_NFIX] = {\n')
            for at, to in sorted(fixups):
                f.write('    { %d, %d },\n' % (at, to))
            f.write('};\n\n')
        f.write('#endif /* ACC_RT_HELPERS_H */\n')

    print('[%s: %d bytes, %d entry points, %d internal calls]'
          % (out, len(text), len(entries), len(fixups)))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3])
