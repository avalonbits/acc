#!/usr/bin/env python3
"""Turn assembled runtime helpers into a C table acc can emit.

The helpers are written as assembly and read as assembly. Nothing here is
transcribed by hand: this assembles the source, reads the symbols and the
bytes out of the object, and writes the header gen.c includes. Getting a byte
wrong in a table of opcodes is not something review catches, so the table is
not written by a person.

Each helper must be position independent and self-contained -- no relocations
at all, which this checks. That is what lets acc drop one into an image at
whatever address is free and call it, with nothing to fix up but the call.
"""
import re
import subprocess
import sys


def run(*args):
    return subprocess.run(args, capture_output=True, text=True, check=True).stdout


def main(asm, out, tools):
    obj = out + '.o'
    run(tools + '/ez80-none-elf-as', '-march=ez80+full', asm, '-o', obj)

    relocs = run(tools + '/ez80-none-elf-readelf', '-r', obj).strip()
    if re.search(r'^0[0-9a-f]+\s', relocs, re.M):
        sys.exit('%s: a helper needs relocation, so it cannot be dropped in as bytes:\n%s'
                 % (asm, relocs))

    text = subprocess.run([tools + '/ez80-none-elf-objcopy', '-O', 'binary',
                           '--only-section=.text', obj, '/dev/stdout'],
                          capture_output=True, check=True).stdout

    # Global symbols only: the entry points, in address order.
    entries = []
    for line in run(tools + '/ez80-none-elf-nm', obj).splitlines():
        addr, kind, name = line.split()
        if kind == 'T' and name.startswith('_acc_rt_'):
            entries.append((int(addr, 16), name[len('_acc_rt_'):]))
    entries.sort()

    # Each helper runs to the next one, or to the end.
    spans = []
    for i, (addr, name) in enumerate(entries):
        end = entries[i + 1][0] if i + 1 < len(entries) else len(text)
        spans.append((name, addr, end))

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
        f.write('/* Where each helper starts and ends within rt_code. */\n')
        f.write('typedef struct { short at, end; } RtSpan;\n\n')
        f.write('enum {\n')
        for name, _, _ in spans:
            f.write('    RT_%s,\n' % name.upper())
        f.write('    RT_COUNT\n};\n\n')
        f.write('static const RtSpan rt_span[RT_COUNT] = {\n')
        for name, at, end in spans:
            f.write('    { %d, %d },   /* %s, %d bytes */\n' % (at, end, name, end - at))
        f.write('};\n\n#endif /* ACC_RT_HELPERS_H */\n')

    print('[%s: %d bytes in %d helpers]' % (out, len(text), len(spans)))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3])
