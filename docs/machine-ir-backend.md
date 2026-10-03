# A machine-level backend for opt-acc

opt-acc's SSA form is made into code by three backends today: the first
pass's (the classic one-pass compiler, replayed), the hybrid path's (the
classic emitters with values in registers), and the leaf backend's
(`ssa.c`, every instruction selected from the SSA form). The pick keeps
whichever is cheapest for each function. This is the design for a fourth,
meant to replace the other two SSA backends: one that selects instructions
into a machine-level IR with virtual registers, allocates registers over it,
and lays out the frame after. That is the shape of every optimizing compiler
since the 1980s, and the one agondev's clang has; opt-acc does not have it
yet, and the bytes say that is where most of the distance to agondev is.

## Why

acc compiled by opt-acc is 220,432 bytes of code against agondev -Oz's
179,226 (in-line strings, which agondev keeps in `.rodata` and which come to
the same on both sides, counted apart): 41 KB, 1.23 times. By instruction
class and by code shape:

| what | opt-acc | agondev | more |
|---|---:|---:|---:|
| `ld rr,nn` -- constants and zeros put in a register to be used | 34,855 | 23,573 | +11,282 |
| 24-bit `add`/`sbc`/`inc` -- every value worked at 24 bits in HL | 23,093 | 12,264 | +10,829 |
| frame `(ix+d)` -- values spilled and read back | 41,662 | 34,085 | +7,577 |
| `(hl)` reads and writes -- addresses made, then used | 7,434 | 1,371 | +6,063 |
| jumps | 24,111 | 18,178 | +5,933 |
| `ld r,r` and `ex de,hl` -- values moved to where an instruction wants them | 10,955 | 3,620 | +7,335 |
| `(iy+d)` and `(nn)` -- addressing modes agondev uses instead | 17,779 | 26,160 | -8,381 |
| calls -- agondev's `_frameset`, compare and multiply helpers | 19,428 | 24,948 | -5,520 |

Shapes behind them (bytes, opt-acc against agondev): a signed comparison
biased by `ld bc,0x800000` and two adds, 3,245 against 0; a byte widened to
24 bits and never read wider, 2,692 against 224; a 24-bit zero test, 6,492
against 4,156; the frame made by hand in every function, `push ix / ld ix,0
/ add ix,sp`, 7,119 against agondev's `ld hl,-n / call _frameset`.

None of these is a missing case. They are the leaf backend's model: HL is
the accumulator, every value goes through it at 24 bits, the other operand
is put in DE, and registers are chosen while instructions are, one at a
time. On the 260 functions the leaf backend makes in acc it is 1.35 times
agondev; the classic compiler, on the 435 functions whose first-pass code
the pick keeps, is 1.21 times. The SSA backend is further from agondev than
the one-pass compiler it was meant to beat, and widening what it takes --
calls, longs -- widens that too.

The clang study (agondev -Oz with passes switched off) said the same from
the other side: of agondev's size, the IR passes are worth 14% and code
generation -- selection, the register allocator, its coalescing -- 31%.
opt-acc already has most of the IR side (SSA, locals made values,
inlining by measurement). It has none of the code-generation side.

## Where it sits

```
log of the parser's calls --> SSA form (ssa.c, as now)
                                 |
                                 | value widths (new)
                                 v
                              selection (new) --> machine IR, virtual registers
                                                     |
                                                     | register allocation (new)
                                                     | frame layout, slot sharing (new)
                                                     v
                                                  bytes (insn.c's emitters)
                                                     |
                                     peep.c, relax.c (as now: any backend's bytes)
```

Everything up to the SSA form stays: the log, the CFG, dominators, loops,
phis, liveness, inlining by measurement. Everything after the bytes stays:
the peephole and jump shortening work on whatever made the code. The new
backend is one more way in the pick, `OPTACC_MIR`, weighed with the others
and kept per function only where it wins, so it can be built a slice at a
time without anything getting worse. When it wins everywhere the leaf
backend and the hybrid path are deleted.

## The machine IR

One function: its blocks, in the SSA form's order, each a list of
instructions, with the CFG's edges. Phis are gone by the time instructions
are selected -- each becomes parallel copies at the end of its predecessors,
which the allocator then tries to make nothing by giving both sides the same
register (coalescing).

An instruction is an eZ80 instruction form and its operands:

- the form -- one enum value per encoding family: `ld r8,r8`,
  `ld r24,nn`, `add hl,r24`, `alu8 a,r8`, `alu8 a,n`, `alu8 a,(ix+d)`,
  `ld r24,(ix+d)`, `ld r8,(iy+d)`, `ld a,(nn)`, `inc r24`, `jp cc,label`,
  `call sym`, ... -- which knows its bytes and cycles for each operand
  (the decoder in ssa.c's `ez80_insn` already does), what it reads, what it
  writes, and what it clobbers, flags included;
- operands: a virtual register, a physical register, an immediate, a frame
  object (`(ix+d)` before d is known), a symbol and offset (a relocation or
  a fixup, as the emitters make now), a block.

Virtual registers have a class, the set of physical registers the value may
be in, as agondev's backend has them:

| class | registers | for |
|---|---|---|
| A | A | the 8-bit ALU's left operand and answer |
| R8 | A, B, C, D, E, H, L, IYL, IYH | a byte |
| HL | HL | the 24-bit ALU's left operand and answer |
| R24 | HL, DE, BC, IY | a pointer or an int |
| O24 | DE, BC | the right of `add hl,rr` / `sbc hl,rr` |

A form constrains each operand to a class. `add hl,rr` takes its left in HL,
writes HL, and reads rr from O24 or HL (`add hl,hl`); an 8-bit `and` takes A
and writes A. Where a value must be in HL for one instruction and in DE for
another, the allocator decides whether a move (`ex de,hl`, `ld e,l`, a push
and a pop) or a different choice is cheaper -- which the leaf backend
decided by rule, at the moment it met each instruction.

Registers overlap: L is HL's low byte, and an 8-bit write leaves HL's top
byte as it was. Interference is between register units -- each byte of each
pair, as peep.c already tracks them -- not between whole registers, so a
byte in L and a pointer in DE coexist, and a byte in L and a pointer in HL
do not.

Flags are a register too. A comparison defines F; a branch uses it; nothing
the allocator puts between them may change it.

Frame objects are a parameter's bytes (fixed, above IX), a local whose
address is taken, and a spill. Each has a size and a lifetime, and gets its
offset only after allocation, when objects whose lifetimes do not overlap
share bytes -- the stack-slot sharing that is 2.4% of acc's size in agondev.

Longs and floats are frame objects first, worked on by the runtime's
address-convention routines exactly as PR #70 does, and register pairs
(E:UHL, A:UBC) later, when the allocator can hold them.

## Selection

Selection walks each block's SSA instructions and makes machine
instructions for them. A value used once, by an instruction in the same
block, is not given a register of its own -- it is part of its user's tree,
which is how `fwd` values work today -- so selection sees expression trees
and picks, for each, the cheapest covering by instruction forms: tree
pattern matching with costs (BURS), the cost bytes plus k times weighted
cycles as opt-acc's global policy has it.

That is where the addressing modes come from:

- a local or parameter read is `(ix+d)` in the instruction that wants it,
  not a load into HL first;
- a member through a pointer is `(iy+d)` or `(hl)` with the offset folded,
  a global is `(nn)`, an element of a global array at a constant index is
  `(nn)` too;
- a constant is an immediate -- `cp n`, `and n`, `ld (ix+d),n`, `add a,n`,
  `inc`/`dec` for one -- and an int constant is put in a register only when
  the instruction has no immediate form.

And the value widths come in here. A pass before selection works out, for
each value, how many of its bytes can be other than zero or the sign (known
bits, forwards) and how many any use reads (demanded bits, backwards). A
char compared with a constant is compared in A; a byte read, masked and
stored is never widened; a value known not to be negative is compared
without the sign bias; a test for zero of a value known to be a byte is
`or a`. PR #70's narrowing of longs held as their low three bytes is the
same question one width up, and moves here.

Calls are instructions whose operands are the pushes of their arguments,
whose answer is in HL (A for a byte, E:HL for a long). A callee may change
every register but IX -- an acc function with a local in IY keeps it only
for itself -- but the allocator is told a call takes only A, F and HL: a
value live across one in BC, DE or IY stays there, and the pairs holding
such values are pushed before the arguments and popped back after them
(the arguments come off through DE first). That is a byte or two a pair
each way, where a frame slot is three to store and three to load, and
nothing where no value lives across. setjmp is not called this way: its
second return finds the pushed pairs long gone. The runtime's routines
take their operands in HL and BC and keep every register but A, the flags
and the one they answer in, so a call of one clobbers far less again.

## Register allocation

The eZ80's register file is small and irregular: one accumulator per
width, pairs that are also bytes, a frame pointer, an index register. The
constraints decide more than the colouring does, and nothing here may cost
more than n log n in the size of the function: graph colouring's
interference graph is quadratic, and the first allocator, which built one,
took over an hour on gcc's pr69592 -- 600 statements in one function. So
the allocator is a linear scan:

- each instruction a position in the order the blocks are made, operands
  read at 2i and answers written at 2i + 1, so that an operand's register
  can be its answer's;
- each virtual register an interval from its first position to its last,
  carried on to a loop's end where it lives into the loop from before it
  (the loops' extents in a sparse table, one query an interval);
- the intervals in order of their starts, each given a register that no
  interval still live has, that nothing inside it clobbers (a range-OR
  over positions, a sparse table again), and that no instruction inside it
  needs for itself -- an operand that must be HL, A or BC is a short
  interval fixed to that register, found per unit by binary search;
- a copy's partner's register tried first, which is what makes the copies
  come to nothing -- coalescing by preference, not by merging;
- where no register is free, the cheapest of the live intervals that
  could give one up spilled instead -- a constant or a parameter cheapest,
  being made again or read from its own slot -- or the interval itself;
  never a short one made for one instruction, which no spill shortens;
- the spills rewritten in one pass over the code, reloads and stores
  through short registers of their own, and the scan run again.

The live set is never more than the eleven registers, so each step of the
scan is constant work: the whole is the sort.

## Frames

After allocation the frame is known: its objects, their sizes, whether the
function calls, and whether IX is needed at all. Laid out by size cost:

- no frame, where the function reads no stack argument and keeps nothing in
  memory -- many of acc's small functions;
- `ld hl,-n / call _frameset`, agondev's -Oz form, 8 bytes against 13 for
  the frame made in place (an earlier attempt at it in opt-acc lost the
  speed gate; here it is a cost weighed per function);
- made in place, where that is cheaper by the cost.

## Emitting

Each machine instruction is made by the emitters insn.c already has, the
relocations and fixups as they are made now, the constant pool and the jump
fixups the same. Then peep.c and relax.c, unchanged. Jump tables for dense
switches, and tail merging across blocks, are later passes over the machine
IR -- easier there than on bytes, which is where peep.c does them now.

## Testing

- A verifier over the machine IR after each pass: every operand in its
  class, every register read defined on every path to it, F not clobbered
  between a comparison and its branch.
- `test/test_mir.c`: selection and allocation of small functions, with the
  bytes expected, as `test/test_peep.c` does for the peephole.
- The suites forced through it (`OPTACC_MIR=1 OPTACC_PICK=0`): test/run.sh,
  both conformance suites, zap, and csmith and the fuzzer, where a backend
  this size will have bugs no hand-written case finds.
- The measure of progress: acc's and zap's functions, each against agondev's
  with inlining off on both sides, by instruction class -- the table above,
  for each milestone.

## Milestones

Each one is a PR, kept per function by the pick, gated on the whole corpus
as opt-acc's PRs are.

1. **The machine IR, selection and allocation for functions of ints,
   pointers and chars without calls** -- the leaf backend's first domain.
   The verifier and test_mir.c with it. Measured against the leaf backend
   on the same functions; the target, agondev's sizes on them.
2. **Calls**, and the runtime's routines as the cheaper calls they are.
3. **Value widths**, and selection of 8-bit forms where they allow. The
   known bits are worked out in one pass over the SSA form, in the order
   its instructions are: a byte's widening, a mask, a shift, a remainder,
   a truth, a `_Bool` read -- a phi knowing what all its inputs know, and
   nothing of what comes round a loop, so that the pass is linear. They
   say where a value is a byte (compared and tested in A), where it is not
   negative (compared without the bias) and where a mask keeps nothing it
   has. With them: a signed comparison with a constant by the sign or with
   the constant moved already; &, | and ^ with a constant a byte at a
   time; an index scaled by adds.
4. **Frames**: none, `_frameset`, or in place; slot sharing.
5. **Longs and floats**, first in frame objects as now, then in pairs.
6. **The leaf backend and the hybrid path deleted**, when the pick keeps
   them nowhere; jump tables and tail merging on the machine IR.

## What it should buy

Not a promise: the shapes it removes are about half of acc's 41 KB gap --
values through HL at 24 bits, constants in registers, moves, the sign bias,
the widening -- and frames are 3 to 7 KB more. opt-acc's code for acc is
23% bigger than agondev's, whose backend has this shape. The table above,
made again after each milestone, is how to tell whether it is working.
