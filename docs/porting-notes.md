# Porting tinycc to the Agon

Findings from the initial survey, with the measurements that produced them.
Everything here was measured against agondev's own clang and the real tinycc
tree, not reasoned from documentation.

## The budget

`RAM_START 0x40000`, `RAM_SIZE 0x70000`: **448 KB** for code, data, heap and
stack together. No MMU, no swap.

Measured `.text` for tinycc built by agondev for the eZ80, per file:

    tccgen.c   78,849    tccelf.c   50,696    i386-asm.c  15,314
    tccpp.c    48,337    tccdbg.c   33,234    tccasm.c    13,382
    libtcc.c   26,419    i386-gen.c  7,906    i386-link.c  2,731

    full i386 build: 293,539 text + 62,305 bss

A build that drops debug info, the assembler, ELF output and the run/tools
paths, and adds an eZ80 backend and a flat-binary writer, comes to roughly
185 KB text and 25 KB bss, leaving about 230 KB for heap and stack.

## The data model

agondev's eZ80 C:

| type | size |
| --- | --- |
| `char` | 1 |
| `short` | 2 |
| `int`, pointer, `size_t`, `ptrdiff_t` | **3** |
| `long`, `float`, `double` | 4 |
| `long long`, `long double` | 8 |

`double` really is 4 bytes -- the same as `float`, using the `__f*` helpers.
True IEEE binary64 exists as `long double`, with the `__d*` helpers. Matching
this exactly is what lets acc's output link against `libagon.a`.

### `int` is 24 bits and `long` is 32

The eZ80 therefore has five integer widths (1, 2, 3, 4, 8) where tinycc's
`VT_BTYPE` has four basic types. It needs no fifth: tinycc already carries a
`VT_LONG` flag to tell `long` from `int` at the same base type, so `type_size`
can return 3 for `VT_INT` and 4 for `VT_INT|VT_LONG`.

The risk in that approach is every place that reads `t & VT_BTYPE` and assumes
a size instead of calling `type_size()`. There are 145 `VT_BTYPE` references
in `tccgen.c` and 20 in a backend.

### `CType.t` needs 32 bits

`CType.t` packs flags in bits 0-16 and bitfield position/size in bits 20-31:
**29 bits**. It cannot be repacked into 24 -- dropping TLS, VLAs and long long
bitfields still leaves 25 -- so on a machine whose `int` is 24 bits it has to
become a 32-bit type.

That is affordable. Measured, agondev's clang narrows 32-bit operations whose
results provably fit in 24 bits:

| operation | 32-bit `t` | 24-bit `t` |
| --- | --- | --- |
| `(t & 0x0f)` | 8 insns, 1 call | 8 insns, 1 call |
| `(t & 0x0f) == 5` | 14 insns, 0 calls | 14 insns, 0 calls |
| write storage flags | 18 insns, 3 calls | 11 insns, 2 calls |

Only genuine upper-byte work pays, and that is the cold path.

## Speed: the helper calls are the cost

On this chip a great deal of ordinary C has no instruction to compile to and
becomes a call to a library helper. In tinycc's hot path the worst offender is
the type test, and reading the flag word a byte at a time removes the call:

| | word | byte |
| --- | --- | --- |
| extract basic type | 7 insns + `__iand` | 9 insns, no call |
| test `unsigned` | 11 insns + `__ishru` + `__iand` | 9 insns, no call |
| test `static` | 11 insns + `__ishru` + `__iand` | 9 insns, no call |

Two extra instructions to delete two calls, on a test that runs constantly.
tinycc already routes nearly all of these through macros, so the change is
contained.

## Memory: two 256 KB arenas

    #define TOKSYM_TAL_SIZE (256 * 1024)   /* tccpp.c */
    #define TOKSTR_TAL_SIZE (256 * 1024)   /* tccpp.c */

Allocated eagerly at startup: **512 KB, more than the machine has**. Both are
append-only arenas that grow by chunking, so shrinking them costs nothing.
Measured with massif, dropping both to 16 KB took baseline heap for an empty
translation unit from 576,859 to 183,787 bytes.

What remains of that baseline is mostly `table_ident`, about 8,200 `TokenSym`
pointers from parsing the predefined macros -- 65 KB on a 64-bit host, 24 KB
with 3-byte pointers. `hash_ident` is a further 50 KB of bss at
`TOK_HASH_SIZE 16384`; 4096 would save 38 KB.

Marginal cost is about 254 bytes per line of C on a 64-bit host, which scales
to roughly 110 bytes per line with 3-byte pointers. That puts the ceiling near
**1,000 to 1,500 lines per translation unit**.

## Why not emit assembly for zap

[zap](https://github.com/avalonbits/zap) is a fast eZ80 assembler that already
runs on the Agon, so having acc emit text for it is the obvious idea. It is
the wrong one for the main path, for two reasons.

**tinycc backpatches.** `gjmp()` returns a chain of forward jumps that
`gsym_addr()` later patches by writing addresses into bytes already emitted.
Text cannot be backpatched, and `ind`, the current output offset, is
load-bearing throughout `tccgen.c` for loops, switches and function sizes.

**The round trip is not free.** zap's own baselines put it at about 3,700
lines/sec on an Agon (bbcbasic: 14,757 lines in 3.86 s; rokky: 1,957 in
0.54 s). tinycc emits roughly as much code as `clang -O0` -- measured on one
input, tcc 92,045 bytes against clang's 97,619 at `-O0` and 76,475 at `-Oz` --
which is 3 to 6.5 assembly lines per line of C. A 1,000-line file is then
0.8 to 1.8 seconds in zap alone, plus formatting the text on a chip with no
divide instruction, plus 100-250 KB through the SD card twice. Emitting bytes
directly costs approximately nothing.

What is worth taking from zap is its `isa_table` encodings, as a source-level
reference for the backend, and a `-S` mode emitting zap syntax for debugging
and for hand-tuning hot routines. Neither puts zap in the hot path.

## What building it actually needed

tinycc's core compiles for the eZ80 today. The complete list of blockers:

| blocker | fix |
| --- | --- |
| `fcntl.h`, `unistd.h`, `sys/time.h`, `dlfcn.h`, `semaphore.h` | stub headers, ~40 lines |
| `ssize_t`, `EINTR` undeclared | typedef and define |
| `realpath`, `fdopen`, `getenv`, `strerror` | stub out; no environment or symlinks under MOS |
| `Error: CPU mode is unsupported` | `-Wa,-march=ez80+full` was missing from the compile line |

## Output format

MOS binaries start with a 64-byte header: `jp $040045` (`c3 45 00 04`), a
60-byte name field at offset 4, then `MOS` and a version byte at 0x40. Origin
is `0x40000`. `agondev-setname` writes the name field.

## Finding tinycc's 32-bit assumptions

`CType.t` has to be widened on a machine whose `int` is 24 bits, and the
problem with that change is that it is invisible: on a 32-bit host the widened
type and `int` are the same thing, so every place that should have been
widened and was not still compiles and still works. The compiler cannot help.

What makes it visible is building the type *wider than the host's int*:

    typedef int64_t ctype_t;     /* -DACC_FIND_NARROWING */

Every place that holds a type word in an `int` then becomes a
`-Wshorten-64-to-32` warning. Subtracting the warnings the tree already had
leaves the ones the change is responsible for -- 51 of them, in 3 files -- and
the count is driven to zero. It is not a supported build, only an instrument.

Two things that turned up which a narrower search would have missed:

* `block()`, `unary()` and `tcc_get_dwarf_info()` each reuse one variable for
  both a type word and something narrower (a token, a DWARF offset). Widening
  the variable would have worked and hidden the reuse; each got a second name
  instead.
* `parse_btype`'s `bt` and `st`, and `adjust_bf`'s return value, are type
  words held in `int`.

## Why the equivalence test exists

The widening above was semantics-preserving *except* for one site, where the
replacement of a use was applied by line number after an insertion had already
moved it. The result read an uninitialised variable in `gv()`. It compiled
without a warning, passed every data-model assertion, and produced wrong code
for bitfields, enums, integer promotion and struct return.

`test/equivalence.sh` caught it: five objects out of 135 differed from
pristine tinycc's. Nothing else in the suite noticed. Any change to the shared
core should be run against it.
