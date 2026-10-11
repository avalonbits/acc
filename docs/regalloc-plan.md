# A splitting register allocator for opt-acc's machine-level backend

The machine-level backend (`mir.c`, [machine-ir-backend.md](machine-ir-backend.md))
allocates registers with a linear scan over intervals with lifetime holes.
A value that cannot have a register for all of its life is spilled for all
of it: stored at every write, read back at every read. That is the last
thing between opt-acc and agondev on the one program still slower than
agondev's build, zap, and this is the plan for an allocator that keeps a
value in a register where it is used most and in memory only where it has
to be.

## Why

Built by opt-acc's full pipeline, zap runs at 1.075 times agondev -Oz's
cycles; acc, vi, AED and ez80asm are all at or under 1.0. zap's whole
distance is its encoder: `assemble_line` and the `parse_operand`,
`reg_of_text` and `emit_imm` it calls, 45 million cycles against agondev's
36 million for the same work, where agondev reads all of it into
`assemble_line`. The rest of zap is faster than agondev's.

Three steps toward reading the encoder in place have been made and parked,
each on a branch of its own, each built on the one before:

| branch | what it does | zap |
|---|---|---|
| `opt-inline-goto` | bodies with labels read in place, each copy its own labels | 95.6M: SSA refuses the merged functions |
| `inline-merged` | SSA's shared-slot refusal off; the machine IR tried where the first way fails; the frame's bytes counted once | 83.9M: the merged `assemble_line` is made by the machine IR |
| `addr-forward` | a local whose address only feeds a body's pointer becomes a value -- `p` in `parse_operand(&a, &p, e)` | 84.9M: `p` is a value, and spilled whole |
| `split-alloc` | the existing splitting allocator taken through the merged function | 95.5M: 1343 parts, a move at every cut |

against 81.9M on main. The merged `assemble_line` is 15 KB and spends 40%
of its time on the frame: values live across the calls it makes are
spilled for their whole lives, `p` among them once it is a value. The
eZ80 has no register a call keeps but IX, so everything live across a
call is in memory across it -- in agondev's code too -- but agondev keeps
such a value in a register between the calls and stores it once around
each, where the linear scan reads it from memory at every use.

## What has been tried

- **A register oracle** -- every choice the linear scan makes searched,
  each measured: zap 0.27% at most. Choices within the scan are not the
  lever.
- **A priority allocator without splitting** (branch `mir-greedy`): slower
  everywhere. Without splitting, a hot loop value evicts a long one that
  then has nowhere at all.
- **Load reuse and loop-invariant code motion** (`mir-load-reuse`,
  `mir-licm`): the reused or hoisted values were spilled; slower.
- **Exit splitting** (`mir-exit-split`): a gain on one loop, a loss on the
  rest.
- **The splitting allocator** (way 5): cuts a value at its next use when
  its register is wanted, and resolves the cuts with moves. Through the
  merged encoder it makes 1343 parts and a move at nearly every one.

## Design

An allocator in the shape of LLVM's greedy allocator, sized for this
machine: a priority queue of live ranges, assignment with eviction, and
splitting chosen by the structure of the code -- calls and loops -- rather
than by the next use.

1. **Weights from block frequency.** Each range's spill weight is the sum
   of its uses weighted by the estimated frequency of their blocks
   (`ssa_freq`'s, the pick's), divided by its length: what keeping it in
   memory would cost.
2. **Priority.** Ranges are taken heaviest first, so that a loop's values
   choose before the long, cold ones they would otherwise be evicted by.
3. **Assign or evict.** A free register for the whole range; or one whose
   holders all weigh less, evicted back into the queue.
4. **Split where neither works**, cheapest first:
   - **around calls** -- the range cut into the stretches between the
     calls it lives across: each stretch may have a register, the value
     stored once before a call and read once after it where a stretch
     after reads it;
   - **around loops** -- a value kept in a register through a loop where
     the loop's pressure allows, stored and read at the loop's edges;
   - **around one instruction** -- a value moved out of the register an
     instruction must have (HL for a 24-bit add, IY for `(iy+d)`, A for a
     byte operation) and back after.
   Each piece goes back into the queue with its own weight.
5. **Spill placement.** A range left in memory is stored once, after its
   definition, and read at the start of each stretch that reads it --
   never inside a loop it is not changed in.
6. **Resolution.** The existing machinery joins the pieces:
   `split_resolve`'s moves on edges and at cuts, `pcopy_to_memory` for a
   join's copy into memory, `check_joined` verifying every read sees the
   value it should.

Every pass stays linear or n log n in the code (the queue is a heap; the
call and loop structure is computed once per function).

## Milestones

Each is measured on the five programs (`test/apps.sh`, cycles and code)
and the corpus (`test/perf.sh`, `test/size.sh`), against main the same
day, before the next begins.

1. **Weights and priority**, no splitting yet beyond the existing linear
   scan's: the queue and eviction on the existing intervals. Must not lose
   to the linear scan on the corpus.
2. **Splitting around calls.** The first that should move the merged
   encoder: `p`'s 3.8M cycles of reads and 1M of writes come down to a
   store and a read per call.
3. **Splitting around loops**, and the loop-aware spill placement.
4. **Splitting around fixed registers.**
5. **In the pick**: the new allocator as a way of its own, the pick's
   rules deciding, the parked branches merged on top -- reading the
   encoder in place, forwarding `p` -- where they then win.

## When to stop

If milestone 2 does not bring the merged encoder under the 81.9M cycles
zap takes without reading it in place, the encoder's distance is not in
the allocator, and the work stops there.
