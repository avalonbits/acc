# acc - a C compiler that runs on the Agon

acc is [tinycc](https://github.com/TinyCC/tinycc) retargeted to the eZ80 and
cut down to fit an Agon Light: a C compiler that runs **on** the machine, not
on a PC that cross-compiles for it.

It is early. Nothing generates eZ80 code yet; see the status table below.

## Why tinycc

It is a whole C99 compiler in about 23,000 lines, it compiles in one pass with
no intermediate representation, and its backend interface is roughly
twenty-five functions. Nothing else that is a real C compiler is small enough
to fit in 448 KB alongside its own working set.

## The three builds

The port is kept honest by building the same tree three ways. The first two
are not alternatives to the third; they are how it is debugged.

| build | runs on | targets | what it is for |
| --- | --- | --- | --- |
| `acc-i386` | host | i386 | Unchanged tinycc semantics, so tinycc's own test suite still applies. Says whether a change to the shared core broke something. |
| `acc` | host | eZ80 | The cross-compiler. eZ80 type sizes and ABI, running where there is memory and a debugger. |
| `acc.bin` | Agon | eZ80 | The point of the exercise. Built with [agondev](https://github.com/AgonPlatform/agondev). |

    make          # acc-i386 and acc
    make test     # the suite, and the equivalence check below

`make test` asserts each target's data model, runs every program in
`test/exec` on an emulated Agon and checks what it printed, and compares
acc-i386's output against pristine tinycc's byte for byte over tinycc's own
corpus. The
i386 target is unchanged tinycc semantics, so any change meant to be a port
and not a behaviour change has to leave every generated object identical. The
reference tree is cloned into `test/ref` on first run; point `ACC_REF_TCC` at
an existing checkout to skip that.

## Status

| | |
| --- | --- |
| Vendored tinycc, host build, test harness | done |
| eZ80 data model (24-bit `int`, 32-bit `long`) | done |
| `CType.t` widened to 32 bits | done |
| Objects agondev's binutils can link | done |
| eZ80 backend: `int`, pointers, control flow, calls | done |
| eZ80 backend: `char` and `short` | done |
| eZ80 backend: `long`, floats, `long long` | not started |
| acc's own linker and flat MOS output | done |
| Builds with agondev to run on the Agon | compiles; not yet run on hardware |

acc compiles C to eZ80 machine code today for `char`, `short`, `int`,
`unsigned` and pointers, with the full range of operators, control flow,
function calls and recursion. Anything wider refuses rather than
emitting something that would assemble and misbehave.

acc links too. It reads `libagon.a`, lays the image out itself and writes the
flat MOS binary, with no linker script and no `ld` -- which it has to, because
neither exists on the Agon. The images come out byte-identical to agondev's
`ld` on every program in the test suite.

    $ cat hello.c
    int printf(const char *, ...);
    int fact(int n) { int r = 1; while (n > 1) { r = r * n; n = n - 1; } return r; }
    int main(void) { printf("8! = %d\r\n", fact(8)); return 0; }

    $ bin/acc -nostdlib -o hello.bin hello.c -L ~/agondev/lib -lagon
    $ test/agon.sh hello.bin
    8! = 40320

agondev is still needed for `libagon.a` itself -- acc has no C library of its
own -- but for nothing else in the build.

## The constraints that shape it

The Agon gives a program 448 KB (`RAM_START 0x40000`, `RAM_SIZE 0x70000`) for
code, data, heap and stack together. There is no MMU and no swap. agondev's
eZ80 C has a 24-bit `int`, 3-byte pointers, a 32-bit `long`, and a `double`
that is the same 4 bytes as `float`.

Two consequences run through the whole port, both written up in
`docs/porting-notes.md`:

* tinycc's `CType.t` needs 29 bits and cannot be repacked into 24, so it has
  to be widened to a 32-bit type on a machine whose `int` is 24.
* Ordinary C that looks like arithmetic compiles to helper calls on this chip.
  A type test written `t & FLAG` is a `call __iand`; the same test read as a
  byte is not a call at all.

## Licence

tinycc is LGPL 2.1 and acc inherits it; see `LICENSE`. The upstream commit
this tree was taken from is recorded in `docs/UPSTREAM`.
