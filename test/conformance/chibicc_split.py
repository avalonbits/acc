#!/usr/bin/env python3
# chibicc's tests, as the conformance suite takes them: a program per
# ASSERT.
#
#   chibicc_split.py <chibicc's test directory> <out directory>
#
# chibicc's test files are each a main of ASSERTs, most of them strict C99
# and many not -- a GNU statement expression, `({ ... })`, is in over six
# hundred of them -- and the suite's filter takes or leaves a whole file.
# So each file is written out once per ASSERT, with the others taken out
# of its main and everything else left as it was: its declarations, and
# main's statements that are not ASSERTs, which the ones after them may
# need. The filter and agondev's reference then judge each ASSERT on its
# own. A file with no ASSERT is written out whole.
#
# chibicc links its tests with test/common, which defines what some of
# them call. Here a test is one file, so common is put after the test --
# where a typedef the test repeats has already been given, as a second one
# C99 does not allow -- and only what the test does not define itself: its
# `static_fn` is its own. Only a test that names something of common's
# gets it.
#
# "test.h" becomes chibicc.h, the suite's own; see there.

import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def scan(text):
    """The text as (kind, piece) runs: code, or a comment, string or
    character literal, which the splitting below must not look inside."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(('skip', text[i:j]))
        elif text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(('skip', text[i:j]))
        elif c in '"\'':
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == '\\' else 1
            j += 1
            out.append(('skip', text[i:j]))
        else:
            j = i
            while j < n and not (text[j] in '"\'' or text.startswith('/*', j)
                                 or text.startswith('//', j)):
                j += 1
            out.append(('code', text[i:j]))
        i = j
    return out


def statements(body):
    """main's body cut into its statements at depth 0: each ends at a `;`
    outside parentheses and braces, or at the `}` that closes a block.
    Joined back together they are the body again."""
    items, cur, depth = [], [], 0
    for kind, piece in scan(body):
        if kind == 'skip':
            cur.append(piece)
            continue
        start = 0
        for k, ch in enumerate(piece):
            if ch in '({[':
                depth += 1
            elif ch in ')}]':
                depth -= 1
            if depth == 0 and (ch == ';' or ch == '}'):
                cur.append(piece[start:k + 1])
                items.append(''.join(cur))
                cur, start = [], k + 1
        cur.append(piece[start:])
    items.append(''.join(cur))
    return items


def code_only(text):
    return ''.join(p if k == 'code' else ' ' * len(p) for k, p in scan(text))


def main_body(text):
    """Where main's body is: the offsets just inside its braces."""
    code = code_only(text)
    m = re.search(r'\bint\s+main\s*\([^)]*\)\s*\{', code)
    if not m:
        return None
    depth, i = 1, m.end()
    while depth:
        depth += {'{': 1, '}': -1}.get(code[i], 0)
        i += 1
    return m.end(), i - 1


def top_level(text):
    """A file's top-level declarations and definitions, each with the name
    it gives: a function's, a variable's, a typedef's."""
    code = code_only(text)
    items, start, depth = [], 0, 0
    for i, ch in enumerate(code):
        if ch in '({[':
            depth += 1
        elif ch in ')}]':
            depth -= 1
        if depth == 0 and ch in ';}':
            piece = text[start:i + 1]
            flat = code[start:i + 1]
            # A function's body ends at its `}`; a struct's does not end
            # the declaration, which goes on to its `;`.
            if ch == '}' and not re.search(r'\)\s*\{', flat):
                continue
            items.append((defined_name(flat), piece))
            start = i + 1
    return items, text[start:]


def defined_name(flat):
    flat = re.sub(r'#[^\n]*', '', flat).strip()
    if flat.startswith('typedef'):
        m = re.search(r'(\w+)\s*;\s*$', flat)
        return m.group(1) if m else None
    m = re.match(r'[^=({;]*?(\w+)\s*\(', flat)          # a function
    if m and '=' not in flat[:m.end()]:
        return m.group(1)
    m = re.match(r'[^=;]*?(\w+)\s*(\[[^]]*\]\s*)*(=|;)', flat)
    return m.group(1) if m else None


def common_for(test_text, common_items):
    """What of common a test gets: nothing, if it names none of it; else
    what it does not define itself, in common's order."""
    names = {n for n, _ in common_items if n}
    used = set(re.findall(r'\b\w+\b', code_only(test_text)))
    if not names & used:
        return ''
    own = {n for n, _ in top_level(test_text)[0] if n}
    return ''.join(piece for n, piece in common_items if n not in own) + '\n'


def main():
    src, out = sys.argv[1], sys.argv[2]
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(out)
    shutil.copy(os.path.join(HERE, 'chibicc.h'), out)
    for name in os.listdir(src):
        if name.endswith('.h') and name != 'test.h':
            shutil.copy(os.path.join(src, name), out)

    with open(os.path.join(src, 'common')) as f:
        common = f.read()
    common = re.sub(r'#include[^\n]*\n', '', common)
    common = re.sub(r'void assert\(int expected, int actual, char \*code\)'
                    r' \{.*?\n\}\n', '', common, flags=re.S)
    common_items = top_level(common)[0]

    for name in sorted(os.listdir(src)):
        if not name.endswith('.c'):
            continue
        with open(os.path.join(src, name)) as f:
            text = f.read().replace('#include "test.h"',
                                    '#include "chibicc.h"')
        base = name[:-2]
        tail = common_for(text, common_items)
        where = main_body(text)
        items = statements(text[where[0]:where[1]]) if where else []
        asserts = [k for k, s in enumerate(items)
                   if code_only(s).strip().startswith('ASSERT(')]
        if not asserts:
            with open(os.path.join(out, base + '.c'), 'w') as f:
                f.write(text + tail)
            continue
        for number, keep in enumerate(asserts, 1):
            body = ''.join(s for k, s in enumerate(items)
                           if k == keep or k not in asserts)
            with open(os.path.join(out, '%s-%d.c' % (base, number)), 'w') as f:
                f.write(text[:where[0]] + body + text[where[1]:] + tail)


if __name__ == '__main__':
    main()
