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

    make          # acc-i386
    make test     # the suite

## Status

| | |
| --- | --- |
| Vendored tinycc, host build, test harness | done |
| eZ80 data model (24-bit `int`, 32-bit `long`) | in progress |
| `CType.t` widened to 32 bits | not started |
| eZ80 backend (`ez80-gen.c`) | not started |
| Flat MOS binary output | not started |
| Builds with agondev | not started |

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
