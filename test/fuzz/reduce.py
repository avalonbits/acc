#!/usr/bin/env python3
"""Cuts a program test/fuzz.sh kept down to what is needed to show the bug.

    test/fuzz/reduce.py <program.c>        # writes <program>.min.c

A statement at a time, then a parenthesised subexpression at a time replaced
by 1, each change kept if acc still gets the program wrong -- a different
answer from agondev's, or an internal error. What is left is usually one
statement of a few operands, which is small enough to read the code acc
emits for it.

Run from anywhere; it works in the repository's root.
"""

import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def run(args):
    return subprocess.run(args, cwd=ROOT, capture_output=True, text=True)


def verdict(text, work):
    """What acc does wrong with this program, or None if nothing."""
    src = os.path.join(work, "t.c")
    with open(src, "w") as f:
        f.write(text)
    built = run(["bin/acc", src, "-o", src + ".a", "-x"])
    if built.returncode:
        return "internal" if "internal" in built.stderr else None
    got = run(["test/agon.sh", src + ".a"]).returncode
    if run(["test/oracle.sh", src, src + ".r"]).returncode:
        return None
    want = run(["test/agon.sh", src + ".r"]).returncode
    if want == 124:
        return None                 # agondev's build hung: no reference
    return "wrong" if got != want else None


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: reduce.py <program.c>")
    path = sys.argv[1]
    with open(path) as f:
        text = f.read()

    with tempfile.TemporaryDirectory() as work:
        kind = verdict(text, work)
        if kind is None:
            sys.exit(f"{path}: acc gets this one right")

        def still(candidate):
            return verdict(candidate, work) == kind

        # Statements first: most of a program has nothing to do with it.
        lines = text.split("\n")
        changed = True
        while changed:
            changed = False
            for i, line in enumerate(lines):
                if line.startswith("    sum = sum ^") or (
                        line.startswith("    *p_") and "=" in line):
                    trial = lines[:i] + lines[i + 1:]
                    if still("\n".join(trial)):
                        lines = trial
                        changed = True
                        break
        text = "\n".join(lines)

        # Then operands, innermost parenthesis first.
        changed = True
        while changed:
            changed = False
            for m in re.finditer(r"\([^()]*\)", text):
                trial = text[:m.start()] + "1" + text[m.end():]
                if still(trial):
                    text = trial
                    changed = True
                    break

    out = re.sub(r"\.c$", "", path) + ".min.c"
    with open(out, "w") as f:
        f.write(text)
    print(f"{out}: acc still gets it {kind}")
    for line in text.split("\n"):
        if line.startswith("    sum = sum ^") or line.startswith("    *p_"):
            print(line)


if __name__ == "__main__":
    main()
