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

## Where the eZ80 build stands

acc's tree compiles for the eZ80 with agondev today, with no errors, and
`ctype_t` really is 32 bits there while `int` is 24 -- checked with a
`_Static_assert` built by agondev's own compiler, including that the bitfield
bits at 26-31 survive.

    text 236,459   bss 62,793   total 299 KB of the 448 KB budget

That is the whole compiler including debug info, ELF output and the run and
tools paths, none of which the Agon can use. The trimming from here is
subtraction rather than rewriting:

| drop | saves |
| --- | --- |
| `tccdbg.c` (DWARF and stabs) | 33 KB text |
| `TOK_HASH_SIZE` 16384 to 4096 | 38 KB bss |
| ELF output, replaced by a flat MOS writer | most of 50 KB text |
| `tccrun.c`, `tcctools.c` | 8 KB text |

Which lands near 185 KB text and 25 KB bss, leaving about 230 KB for heap and
stack -- the figure the budget section above assumes.

## The eZ80 backend

### What the chip's own runtime settles

agondev ships a soft-arithmetic library and the backend calls it rather than
inlining anything. The convention, read off agondev's own output rather than
documentation, is uniform and makes the backend small: **the left operand
arrives in HL, the right in BC, and the result comes back in HL.**

| | |
| --- | --- |
| `__imulu` `__imuls` `__idivs` `__idivu` `__irems` `__iremu` | HL op BC -> HL |
| `__iand` `__ior` `__ixor` `__ishl` `__ishrs` `__ishru` | HL op BC -> HL |
| `__ineg` `__inot` | HL -> HL |
| `+` `-` | `add hl,de` / `or a,a; sbc hl,de`, no call |

`__frameset` takes a negative frame size in HL and leaves IX pointing at the
saved IX, so arguments start at `ix+6` and locals are negative. The epilogue is
`ld sp,ix; pop ix; ret`. Arguments are pushed right to left in 3-byte slots and
the caller cleans up.

### IY is the indirection register

Only HL can be the base of `ld rr,(hl)`, so dereferencing an address held in DE
or BC appears to need a move to HL -- and HL is very often holding something
the register allocator still wants. `*a = *b` is the case that shows it: the
destination address is in HL, evaluating `*b` moves `b` on top of it, and the
store lands in the wrong object.

The backend reserves IY and uses `ld rr,(iy+0)` instead. Nothing tcc tracks
ever lives there, so it costs a prefix byte and no spills. That is also why
`NB_REGS` is 3 and not 4: the optimization guide's measurement is that one
index register is the budget inside a loop, and IX is already the frame
pointer.

### A compare must not destroy its left operand

x86 compares with an instruction that does not write back. The eZ80 subtracts,
and tcc counts on the left operand surviving: a `switch` loads the value once
and compares it against every case in turn. A destructive compare tests `x`,
then `x-1`, then `x-1-2`, and `switch(2)` falls to `default`.

`push hl` before and `pop hl` after costs two bytes and fixes it. Neither
touches the flags, and `__setflag` -- called on PE to repair the sign flag
after an overflowing subtract, which is what makes a signed compare readable
off S -- only uses BC and AF.

### Three bugs worth remembering

All three produced code that assembled, linked and disassembled plausibly.
None would have been found by reading it.

* **A displacement added twice.** When an address is already in a register,
  `c.i` is not a displacement -- it still holds whatever the value had before
  `gv()` put it there. Adding it read six bytes past every parameter.
* **`mov_rr` used `ex de,hl`.** A byte shorter than push/pop, but it *swaps*,
  and the allocator is entitled to believe the source still holds its value.
* **VT_CMP set by hand.** `jtrue` and `jfalse` share storage with `c.i`, so
  assigning only `r` and `cmp_op` leaves the last constant looking like a
  pending jump chain. `n > 1` left a 1 there and tcc patched a jump at offset
  1, in the middle of the prologue. `vset_VT_CMP` exists for this.

### Relocations and nocode_wanted

`jp` is absolute, so every forward jump inside a function needs a relocation
against the text section symbol. `greloca` drops a relocation while
`nocode_wanted` is set -- and a forward jump is very often patched in exactly
that state, because the code after an unconditional jump is unreachable and
`gsym` only clears the flag after patching. The jump was emitted while the
code was live, so its relocation is real; `put_elf_reloca` records it without
the check.

## Linking, and the flat MOS image

acc lays out and writes the image itself rather than calling a linker. It has
to: there is no `ld` on the Agon and no linker script. What it produces is
byte-identical to agondev's `ld` on every program in the suite, which is the
test worth having -- a program that merely runs proves much less, since most
of an image is never reached.

Getting there needed five things, all of them discovered by the link failing:

* **RELA, e_flags 0x84, leading underscores, no .eh_frame** -- see above.
* **An undefined symbol that nothing references is not an error.** ld reports
  undefined *references*, not undefined symbols. Several objects in libagon.a
  carry a marker symbol like `__ixor.hijack_lxor` that no relocation mentions,
  and pulling one in for the function it does define used to fail the link.
* **The entry symbol has to be declared undefined before the library is
  scanned**, or crt0.o is never pulled out of the archive -- nothing in a
  program references its own entry point. tinycc already does this for PE;
  the eZ80 needs it for the same reason.
* **The linker script's symbols have to come from somewhere.** crt0.o
  references eighteen that no object defines: `__stack`, `___low_bss`,
  `___len_bss`, `___run_clearbss`, `___heapbot`, `___heaptop`, and the init
  and fini counts and array ends. acc defines them from the same expressions
  agondev's `linker.conf` uses.
* **The header is crt0's, not acc's.** The first 0x45 bytes of crt0.o's
  `.init` section *are* the MOS header -- which is why that section is
  64-byte aligned and has to be placed first. acc emitting its own produced
  two headers, and the entry jumped into the second one's name field. All acc
  does is fill in the name, which is what agondev's separate setname step does.

Two layout details matter for byte-identity, and neither is in the script:

* The order inside `.init` is `.init .fini .init.bss .init.args`, not the
  order `*(.init .init.args .init.bss .fini)` lists. Within one wildcard GNU
  ld places input sections in the order they occur in the object.
* Nothing is aligned. The eZ80 has no alignment requirement and the script
  aligns nothing, so honouring `sh_addralign` inserted padding the reference
  does not.
