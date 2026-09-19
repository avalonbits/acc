# Name lookup in assembly: the plan

Scanning an identifier and finding it in the name table is the most
expensive thing acc does. On names.c, the realistic benchmark input, the
profile puts `name_intern` at 18% of the cycles and `next`, most of whose
own time is the loop that finds where a name ends, at another 18%. clang's
eZ80 output for both loops is poor, and every attempt to coax better code
out of it in C has either lost or flipped between good and bad with
changes that should not matter. This is the plan for writing the whole
lookup in assembly, with the C kept for the host build.

No code yet. A prototype of half of it was built and measured; its numbers
are below, along with why it lost and what the full version would change.

## Where the cycles go

Measured with the cycle-counting build (`make -f Makefile.agon CYCLES=1`)
and a profile from a patched emulator that charges each instruction's
cycles to its address, at commit c823b79:

| input   | cycles/byte | `next` | `name_intern` | `strncmp` |
|---------|------------:|-------:|--------------:|----------:|
| names.c | 350.0       | 18%    | 18%           | 3.5%      |
| big.c   | 567.2       | 15%    | 12%           | 2.3%      |

Per character of an identifier, the C costs about:

- **45 cycles to find the name's end.** Sixteen instructions a character,
  among them a store of the global `cursor` every time round and a push
  and pop that exist only because clang ran out of registers.
- **70 cycles to hash it.** The base pointer and the count come back from
  the frame on every character, and the table index is built with a
  24-bit add.

## What was tried in C, and why it stopped

| attempt | result on names.c |
|---|---|
| hash walks a pointer instead of an index | 374.8 -> 361.4, **kept** |
| scan the name in a local rather than `cursor` | 361.4 -> 367.4: the local was spilled |
| fuse scan and hash into one loop | 442.5: `high + high` became `__ishl` and `__iand` calls |
| fused, `high` kept as a 24-bit int | still `__ishl` |
| fused, written as a `for (;;)` with a break | 365.7: index form and spills again |
| an empty `asm` to stop `x * 10` folding (lex_number) | crashed the backend |

The pattern: clang's loop strength reduction turns a walked pointer back
into base plus index, and whether it does depends on things like which
kind of loop is written. There is no stable C formulation to find.

## The prototype

`scan_name`, in assembly, did the scan and the hash in one loop. The two
256-byte tables it reads, `ctype` and `pearson`, were copied into the
assembly file and aligned to 256 bytes, so that indexing one is two
instructions: the table's page stays in D (or B), and the character goes
in E (or C).

```
; HL: the name.  DE: ctype's page.  BC: pearson's page, C the low lane.
; IXL: the high lane.
.Lloop:
    ld   a, (hl)
    ld   e, a
    ld   a, (de)        ; ctype
    and  a, 3           ; letter or digit
    jr   z, .Ldone
    ld   a, ixl
    add  a, a
    add  a, (hl)        ; high = high * 2 + c
    ld   ixl, a
    ld   a, (hl)
    xor  a, c
    ld   c, a
    ld   a, (bc)        ; low = pearson[low ^ c]
    ld   c, a
    inc  hl
    jr   .Lloop
```

Fifteen instructions a character, none of them a call. On big.c the loop
took 0.74 M cycles for every identifier character of the compile, against
about 2.6 M for the C scan and hash it replaced.

The whole compile did not get that. With the C around it -- `next` calling
it, then a C function turning the two lanes into a bucket, then the probe
loop -- names.c went 350.0 -> 332.0 (-5.1%), text.c 488.3 -> 486.3, and
big.c **567.2 -> 581.8 (+2.6%)**. The glue ate it: the C wrapper that
finished the hash cost 0.90 M on big.c, the probe loop split out of
`name_intern` so both entries could share it cost 1.07 M, and `next`
itself got no cheaper, because clang spilled around the new call.

So half of it in assembly is not a win. The remaining overhead is all in
the calls and the frames between the pieces, which is what doing all of
it in one routine removes.

## The full version

One routine, `name_lookup`, called from `next` in place of the scan loop
and `name_intern`:

1. **Scan and hash.** The loop above.
2. **Finish the hash and find the bucket.** Mix in the length, then the
   second Pearson lookup, then the two masks. A dozen instructions, once
   per name.
3. **Probe.** Walk the buckets from there. For each one holding a name,
   compare it with the text byte by byte, stopping at the first
   difference. That is where the stored name's terminator stops it too, so
   it never reads past the stored name; that is the property the C keeps
   by using `strncmp`, and the reason `memcmp` is wrong. A match returns
   its reference. The C paid a call to `strncmp` for every probe, 3.5% of
   names.c.
4. **Not found: back to C.** Return with the bucket and let a C function
   append the name to the arena, grow the table if it is half full, and
   store the reference. That path runs once per distinct name, a few
   hundred times a compile, and holds the only allocation.

The keyword test after it -- `tok_name < kw_limit`, then the token byte in
front of the name -- can stay in `next`, in C.

### Registers

Inside `name_lookup`, and not touching IX's saved value: HL walks the
text; DE and BC hold the two table pages for step 1 and are free after
it; IXL holds the high lane, with IX pushed on entry and popped on exit.
IY is free throughout, and suits the bucket pointer in step 3. agondev's
convention lets a callee use every register but IX, which test/abi.sh
pins: the caller keeps nothing else live across a call.

### The hash changes

The C hash seeds the low lane with the length, which the scan does not
know until it reaches the end. The prototype started both lanes at zero
and mixed the length in at the end instead. On the hash test's 2,000
names this probes 1.21 times a lookup, where the current hash probes
1.46-1.50. The C in `name_home` has to change the same way, since
`buckets_rehash` and the host build both go through it.

Two things follow from that:

- `test/test_hash.c` needs a new name for its arena-end case, the one
  whose probe must pass the last short name in the arena. It is found by
  the same search, and has to be found again whenever the hash or the
  table's first size changes.
- The output does not depend on the hash, since names are compared, never
  ordered by it. So `test/target.sh` -- the Agon build's output against
  the host build's -- remains a complete check that the two agree on
  every name.

### Build

- `src/lex_agon.s`, assembled by Makefile.agon and linked into acc.bin
  only. The tables live there and nowhere else: the C keeps its own copy
  of `pearson` for the host and for `buckets_rehash`, and a check in
  `make test` compares the two byte for byte, so they cannot drift.
- In `lex.c`, `#ifdef AGONDEV` chooses between the call and the C loop.
  The host build and the sanitized test build keep running the C, which
  is what the address sanitizer can see into.

## Tests

- **Equivalence of the hash.** Run the assembly's hash on the emulator
  over the hash test's names and compare the buckets with the C's. A
  wrong lane shows up as more probes, not as wrong output, so nothing
  else would catch it.
- **The over-read.** On the Agon there is no sanitizer. A test program
  that ends the arena exactly on a short name, as the host test does,
  then looks up a longer name that probes through it. It does not prove
  the absence of an over-read, but it catches the regression that matters:
  a compare that runs to the looked-up name's length.
- **The suite as it stands.** `test/target.sh`, `test/run.sh` and the
  benchmark's own check that every input still returns 42.
- **helpers.sh.** A rule that `next` calls `name_lookup` on the Agon
  build, so an edit cannot quietly return it to the C.

## What decides it

Measured with the counting build over all sixteen inputs, against the
commit before:

- **Go** if every input is faster and names.c is faster by 8% or more.
  The upper bound, if the calls and frames cost nothing, is about 9.5%:
  1.9 M of big.c's 19.6 M cycles over two compiles.
- **Stop** if any input is slower. That is what the half version did, and
  it would mean the calls and frames still cost more than the assembly
  saves.

Either way the numbers go in the commit, as for every other optimization.

## Measuring it

The counting build and `test/bench.sh` are in the tree. The per-instruction
profile needs a patched copy of the emulator (fab-agon-emulator,
`agon-ez80-emulator/src/agon_machine.rs`). The patch:

- keeps a table of 0x80000 cycle counts;
- adds each instruction's cycle-counter delta to the entry for its
  address, less 0x40000;
- writes the nonzero entries to `$AGON_PROFILE` when the machine is
  stopped through IO port 0.

To use it:

1. Build it with `cargo build --release -p agon-cli-emulator`.
2. Copy the binary into a copy of a release build's directory, for its
   firmware, and point `ACC_EMU` at that directory.
3. For symbols, relink `obj/agon-cycles/*.o` without `--oformat binary`
   and read the ELF with `nm -n`.

## Risks

- **Two versions of one algorithm.** Mitigated by the equivalence test,
  and by the hash being the only shared state. The probe and the compare
  just follow the table.
- **Assembly is harder to change.** The routine is about 60 instructions,
  and `name_intern` has not changed in substance since the arena and the
  table were designed.
- **Portability of acc itself.** It stays C for every build but the Agon
  one; nothing here moves acc further from being compiled by agondev, or
  one day by itself.
