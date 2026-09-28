# Should acc use ELF objects instead of its own format?

Short answer: no. ELF would make acc's objects and libraries 1.3–1.6 times
bigger and acc's own image several kilobytes bigger, and the main thing it
promises, linking agondev-built code on the Agon, doesn't work without also
matching agondev's runtime. What's worth doing instead is dropping unused
externs from objects (16% of libc.a) and a small extension so assembly
libraries built by zap can be expressed.

## Background

acc used ELF once. The tinycc port read agondev's objects and linked
libagon.a byte-identically to ld (55dccbd, fb21665). The rewrite switched to
the ACC format (d3eb463) for three-byte numbers throughout, one read per
object, a sorted archive index, dependency records, and later per-function
items (d77dc5f). This checks whether that trade still holds.

The question came up because zap is going to produce relocatable objects so
assembly libraries can be called from C built with either agondev or acc.
Both compilers use the same calling convention (test/abi.sh), so the only
difference between the two targets is the object format. zap will write both
ELF and ACC objects from one shared model.

## Size

Method: 25 of lib/*.c compiled with agondev's clang (`-Oz`, with and without
`-ffunction-sections -fdata-sections`) to get ELF's real per-entry costs; the
other five don't compile with clang because of `va_list` in include/stdio.h.
Then ELF carrying exactly what bin/libc.a carries (the same code, symbols,
relocations, items and dependencies) was modelled from those costs. The
model reproduces agondev's real files: its per-entry costs give the 37.4 KB
of tables measured in the 25 real objects.

| all 30 members of libc.a | size | overhead | vs ACC |
|---|---|---|---|
| ACC v5, as acc writes it today | 109.7 KB | 38.9 KB | 1.19x |
| ACC v5, unreferenced externs dropped | 91.9 KB | 21.1 KB | 1.00x |
| ELF, items and deps in custom sections | 120.9 KB | 50.1 KB | 1.32x |
| ELF, one section per function (ELF's usual way of linking per function) | 147.7 KB | 76.9 KB | 1.61x |

(Code and data are 70.8 KB in every row.)

Per entry:

| | ELF | ACC |
|---|---|---|
| symbol | 16 bytes + name + `_` | 7 + name |
| relocation | 12 | 6 |
| item | about 110 bytes (a section header and its relocation section's header, a section symbol, names) | 3 |
| file | about 480 bytes | 28 |

acc reads whole members from the card when linking. At about 182 KB/s on
real hardware, linking against much of libc would take 0.15–0.3 s longer
with ELF, and each member held in memory during a link would be bigger.

For comparison, the same 25 files as plain agondev ELF: 57.1 KB, of which
19.7 KB is code (clang's code is smaller than acc's) and 37.4 KB is
everything else. With function sections: 76.0 KB, 56.3 KB of it overhead.

## Cost inside acc

- Image size. The whole object and archive layer, obj.o, is 8.2 KB of
  acc.bin's 177 KB of text. An ELF version needs section header tables,
  symbol and string tables, GNU ar (a big-endian, unsorted symbol index and a
  long-name table), and, to link agondev's objects, the full Z80 relocation
  set (327 lines in the old port's ez80-link.c). Estimate: 12–16 KB. Every
  byte of acc's image comes out of the heap.
- 32-bit fields. Every ELF offset and size is 32-bit, which means `__l*`
  helper calls on the eZ80 unless only three bytes are read deliberately.
- Archive lookup. GNU ar's index is unsorted, so finding a symbol is a linear
  scan or an in-memory sort, where ACR is one binary search.

## Why the benefits mostly don't materialize

- Linking agondev-compiled C on the Agon. The container is the easy part.
  agondev's objects call its runtime helpers (`__frameset`, `__setflag`,
  `__iand`, `__fmul` and many more), which live in the 1.3 MB libagon.a. acc
  has its own runtime under other names, and those helpers don't share
  register conventions. A shared container wouldn't make those objects
  linkable without also matching agondev's runtime.
- One writer in zap instead of two. Real but small: the second writer is a
  few hundred lines on top of a shared model.
- binutils on acc's objects. Useful for debugging, and a host-side acc2elf
  tool would provide it without changing acc on the Agon.

## Recommendations

1. Keep ACC as the object format.
2. Drop externs that no relocation uses when writing an object. 1,289 of
   libc.a's 1,692 symbols are header declarations nothing references:
   17.8 KB, 16% of the library. This is independent of the ELF question.
3. Extend ACC for assembly libraries (what became version 1). Hand-written assembly needs two
   things the format can't express:
   - relocation kinds: the low, high and upper byte of an address, and an
     8-bit PC-relative reference (`jr`/`djnz`) to an external symbol;
   - alignment, for example 256-byte-aligned tables whose page goes in a
     register, which docs/assembly-lexer-plan.md relies on.

   The kind can live in the top bits of the existing 24-bit symbol field of a
   relocation, costing nothing per entry, and alignment is one header field.
   zap will write this format, so its layout should be settled together with
   zap before either side implements it.
4. Optional: an acc2elf host tool, if binutils on acc's objects is ever
   wanted.

## Where the recommendations stand

This was written in the zap session, measured against acc's tree as it was
then, and added here unchanged apart from this section.

1. ACC stays.
2. Dropping unused externs: done in 8c2188b. An object carries only the
   externs a relocation uses.
3. v1: proposed from acc, agreed with zap, and built. src/obj.c's note on
   the format is the specification both sides work from, and
   test/accobj.py writes it independently of acc's writer, for test/accobj.sh
   and as a cross-check of zap's output. As built:
   - the relocation kind in the top 4 bits of the 24-bit target field:
     ABS24, LOW8, HIGH8, UPPER8, PCREL8 and ABS16;
   - a second relocation table of 9-byte entries, `relocs_a`, for an addend
     that does not fit its slot -- which HIGH8 and UPPER8 need, the carry out
     of the low byte mattering -- both tables sorted by address, and each
     address in only one;
   - alignment per item, in the top 4 bits of an item's offset, since a link
     takes items out of their object and packs them; and the bss's, in the
     top 4 bits of bss_len;
   - OBJ_VERSION 1 and one more header field, nrelocs_a;
   - C names spelled as agondev spells them, with a leading underscore:
     `_foo`, `_main`, `_acc_rt_fmul`. A name without one is an assembly
     object's own, which C cannot name.
