#!/usr/bin/env python3
"""Random programs over char and short values, for test/fuzz.sh.

    test/fuzz/narrow.py <dir> <count> <seed>

Each program has one function holding eight variables -- signed and
unsigned chars and shorts, and ints -- as locals and as pointers to its
caller's copies, and folds a dozen random expressions over them into one
value, storing through the pointers between them. Narrow values are where
the code generator has the most ways to go wrong: they are widened on every
load, truncated on every store, may be computed a byte at a time in A, and
their loads use scratch registers. Reading them through pointers and holding
several at once puts all three registers under pressure.

Written to find three bugs that the test cases had never reached, and did in
its first run: a short read through a pointer that cost a value held in DE
its low byte, byte arithmetic inside a parenthesis, and a register freed by
spilling the operand an operator had just put in place.

Only operators that cannot overflow an int are used. A signed overflow is
undefined, and a program that has one may be compiled into anything by
agondev, which is the reference; `*` between two variables overflows 24 bits
easily, so there is none. The same seed always writes the same programs.
"""

import os
import random
import sys

VARS = [
    ("c1", "char", -7), ("c2", "char", 100), ("u1", "unsigned char", 200),
    ("s1", "short", -3000), ("s2", "short", 1234),
    ("w1", "unsigned short", 60000), ("i1", "int", 77), ("i2", "int", -5),
]


def leaf(rng):
    name, _, _ = rng.choice(VARS)
    r = rng.random()
    if r < 0.4:
        return f"*p_{name}"
    if r < 0.8:
        return name
    return str(rng.randint(-50, 300))


def expr(rng, depth):
    if depth == 0 or rng.random() < 0.25:
        return leaf(rng)
    op = rng.choice(["+", "-", "&", "|", "^"])
    return f"({expr(rng, depth - 1)} {op} {expr(rng, depth - 1)})"


def program(rng):
    lines = ["int f(" + ", ".join(f"{ty} *p_{nm}" for nm, ty, _ in VARS) + ") {"]
    lines += [f"    {ty} {nm} = {v};" for nm, ty, v in VARS]
    lines.append("    int sum = 0;")
    for _ in range(12):
        lines.append(f"    sum = sum ^ ({expr(rng, 3)});")
        if rng.random() < 0.5:
            nm, _, _ = rng.choice(VARS)
            lines.append(f"    *p_{nm} = {expr(rng, 2)};")
    lines.append("    return sum;")
    lines.append("}")

    decls = "\n".join(f"    {ty} v_{nm} = {v};" for nm, ty, v in VARS)
    args = ", ".join(f"&v_{nm}" for nm, _, _ in VARS)

    # Every byte of the result reaches the exit status, which is one byte.
    return ("\n".join(lines) + "\n\nint main(void) {\n" + decls
            + f"\n    int r = f({args});\n\n"
            + "    return r ^ (r >> 8) ^ (r >> 16);\n}\n")


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: narrow.py <dir> <count> <seed>")
    out, count, seed = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    rng = random.Random(seed)
    os.makedirs(out, exist_ok=True)
    for k in range(count):
        with open(os.path.join(out, f"n{k:03d}.c"), "w") as f:
            f.write(program(rng))


if __name__ == "__main__":
    main()
