#!/usr/bin/env python3
"""acc's error messages, packed for agondev's build.

    src/msgpack.py <out dir> <source.c>...

acc says what went wrong in sentences, and the sentences are some 20 KB of
acc.bin -- on the Agon, 20 KB of the heap the programs it compiles have to
fit in. This writes each source to <out dir> with the format string of every
call that reports an error packed: the 128 words that save the most, each
with the space after it, become one byte from 0x80 to 0xff, written as an
octal escape so that nothing after it can be read as part of it. And it
writes <out dir>/msgdict.h, the words, which src/fmt.c expands the bytes to
as it reads a format -- so what is printed is what the source says, byte for
byte.

Only the format is packed. A literal passed as an argument is printed through
%s, which is not expanded, and a name or a path given to %s can have bytes of
0x80 and above of its own. The host build is not packed at all.

The words are chosen over every source given, and the same sources give the
same words, so a build is the same from one run to the next.
"""
import os
import re
import sys

# The calls that report an error, and which of their arguments is the format.
FORMAT_ARG = {
    'acc_error': 0,
    'acc_error_prev': 0,
    'REFUSE': 0,
    'acc_error_at': 1,
    'acc_error_pos': 2,
    'acc_error_spot': 2,
}

CALL = re.compile(r'\b(' + '|'.join(FORMAT_ARG) + r')\s*\(')
WORD = re.compile(r"[A-Za-z']+ ?")
NWORDS = 128
FIRST = 0x80

ESCAPES = {'n': '\n', 't': '\t', 'r': '\r', '0': '\0', '\\': '\\', "'": "'",
           '"': '"', '?': '?', 'a': '\a', 'b': '\b', 'f': '\f', 'v': '\v'}


def skip_string(s, i):
    """The index past the literal or character constant starting at s[i]."""
    quote = s[i]
    i += 1
    while s[i] != quote:
        i += 2 if s[i] == '\\' else 1
    return i + 1


def call_args(s, start):
    """The arguments of the call whose '(' is at s[start - 1], as spans
    (begin, end), and the index past its ')'."""
    depth, i, begin, args = 1, start, start, []
    while depth:
        c = s[i]
        if c in '"\'':
            i = skip_string(s, i)
            continue
        if s.startswith('/*', i):
            i = s.index('*/', i) + 2
            continue
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
            if not depth:
                args.append((begin, i))
                return args, i + 1
        elif c == ',' and depth == 1:
            args.append((begin, i))
            begin = i + 1
        i += 1


LITERALS = re.compile(r'(\s*"(?:[^"\\]|\\.)*"\s*|\s*/\*.*?\*/\s*)+', re.S)


def literal_text(arg):
    """The text of an argument that is string literals and nothing else,
    escapes decoded; or None."""
    if not LITERALS.fullmatch(arg):
        return None
    out = []
    for lit in re.findall(r'"((?:[^"\\]|\\.)*)"', arg):
        i = 0
        while i < len(lit):
            if lit[i] != '\\':
                out.append(lit[i])
                i += 1
                continue
            e = lit[i + 1]
            if e in '01234567':
                j = i + 1
                while j < len(lit) and j < i + 4 and lit[j] in '01234567':
                    j += 1
                out.append(chr(int(lit[i + 1:j], 8)))
                i = j
            elif e == 'x':
                j = i + 2
                while j < len(lit) and lit[j] in '0123456789abcdefABCDEF':
                    j += 1
                out.append(chr(int(lit[i + 2:j], 16)))
                i = j
            else:
                out.append(ESCAPES[e])
                i += 2
    return ''.join(out)


def formats(s):
    """(begin, end, text) of every packable format in the source s."""
    for m in CALL.finditer(s):
        args, _ = call_args(s, m.end())
        which = FORMAT_ARG[m.group(1)]
        if which >= len(args):
            continue
        begin, end = args[which]
        text = literal_text(s[begin:end])
        if text is not None and all(ord(c) < FIRST for c in text):
            yield begin, end, text


# A conversion, which is left whole: `'%s' ` is not a quote and the word
# `s' `, and packed that way the % loses its letter.
SPEC = re.compile(r'%[-+ #0]*[0-9]*(?:ll|l|h|z)?[a-zA-Z%]')


def tokens(text):
    """The text as words (each with its space), conversions and the
    characters between."""
    at = 0
    for spec in list(SPEC.finditer(text)) + [None]:
        stop = spec.start() if spec else len(text)
        yield from words(text[at:stop])
        if spec:
            yield spec.group(0)
            at = spec.end()


def words(text):
    at = 0
    for m in WORD.finditer(text):
        if m.start() > 0 and text[m.start() - 1].isalpha():
            continue
        yield from text[at:m.start()]
        yield m.group(0)
        at = m.end()
    yield from text[at:]


def choose(texts):
    """The NWORDS words that save the most, best first."""
    count = {}
    for t in texts:
        for tok in tokens(t):
            if len(tok) > 1 and not tok.startswith('%'):
                count[tok] = count.get(tok, 0) + 1
    gain = [((len(w) - 1) * n - len(w) - 1, w) for w, n in count.items()]
    gain.sort(key=lambda g: (-g[0], g[1]))
    return [w for g, w in gain[:NWORDS] if g > 0]


def pack(text, code):
    return ''.join(chr(code[t]) if t in code else t for t in tokens(text))


def unpack(packed, words):
    return ''.join(words[ord(c) - FIRST] if ord(c) >= FIRST else c
                   for c in packed)


def c_literal(text):
    out = []
    for c in text:
        o = ord(c)
        if c == '"' or c == '\\':
            out.append('\\' + c)
        elif 32 <= o < 127:
            out.append(c)
        else:
            out.append('\\%03o' % o)
    return '"' + ''.join(out) + '"'


def main():
    out_dir, sources = sys.argv[1], sys.argv[2:]
    text = {p: open(p).read() for p in sources}
    words = choose(t for s in text.values() for _, _, t in formats(s))
    code = {w: FIRST + i for i, w in enumerate(words)}

    os.makedirs(out_dir, exist_ok=True)
    for p, s in text.items():
        pieces, at = [], 0
        for begin, end, t in formats(s):
            packed = pack(t, code)
            assert unpack(packed, words) == t
            pieces.append(s[at:begin])
            # The lines it took, kept, so the lines after it keep theirs.
            pieces.append(' ' + c_literal(packed) + '\n' * s.count('\n', begin, end))
            at = end
        pieces.append(s[at:])
        # So that the compiler's messages name the file it came from.
        out = '#line 1 "%s"\n' % p + ''.join(pieces)
        write_if_changed(os.path.join(out_dir, os.path.basename(p)), out)

    header = ('/* Generated by src/msgpack.py. Do not edit. */\n'
              '#define MSG_FIRST 0x%x\n'
              '/* The words, each ended by a NUL, in the order of their '
              'bytes. */\n'
              'static const char msg_words[] =\n' % FIRST)
    header += '\n'.join('    %s "\\0"' % c_literal(w) for w in words) + ';\n'
    write_if_changed(os.path.join(out_dir, 'msgdict.h'), header)


def write_if_changed(path, text):
    """Only when it differs, so that make rebuilds only what changed."""
    try:
        if open(path).read() == text:
            return
    except OSError:
        pass
    open(path, 'w').write(text)


if __name__ == '__main__':
    main()
