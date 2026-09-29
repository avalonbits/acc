# `-O`: the plan

acc compiles in one pass, with a budget of about 600 cycles a byte on the
Agon, and that budget has decided what it can do: every optimisation in
[OPTIMIZATIONS.md](OPTIMIZATIONS.md) works on what it can see as it emits.
`-O` lifts the budget. With it, acc may take as long and as much memory as
it needs, and the goal is code **as good as agondev's or better, on both
size and speed**. It is meant for the host, where the time does not
matter; it still runs on the Agon, slowly. Without `-O`, nothing changes.

Not built. This is what the gap to agondev is made of, measured, and how
`-O` would close it.

## Where the gap is

`test/perf.sh` at a66de89, each program profiled per function on the
cycle-counting emulator, acc against agondev `-Oz`. The split is between
the program's own code and the runtime routines it calls:

| program   | ratio | acc: own code | acc: runtime | agondev: own code | agondev: library |
|-----------|------:|--------------:|-------------:|------------------:|-----------------:|
| zap-basic | 1.58  | 106.8M, runtime included | | 62.5M, library included | |
| sieve     | 1.73  | 11.00M        | 0            | 6.18M             | 0.16M            |
| matmul    | 1.63  | 3.81M         | 2.90M        | 2.17M             | 1.94M            |
| fp        | 2.09  | 5.9M          | 124.4M       | 3.7M              | 58.7M            |
| sort      | 1.18  | 5.94M         | 3.05M        | 3.91M             | 3.74M            |
| lists     | 1.18  | 9.12M         | 0.60M        | 7.91M             | 0.35M            |
| words     | 0.98  | 2.25M         | 0.31M        | 1.68M             | 0.93M            |

Cycles, millions. Two different problems:

- **The program's own code** is 1.2 to 1.8 times agondev's in every
  program: sieve 1.78, matmul 1.76, fp 1.57, sort 1.52, words 1.34, crc
  1.25, interp 1.21, lists 1.15. For the integer programs this is the
  whole loss. It is what `-O` is for.
- **The runtime** loses in floats and in the multiplies, and wins
  everywhere else (crc 13.8M against 21.1M, interp 0.5M against 6.7M,
  wide64 5.5M against 7.1M). Where it loses, it is the routines' own
  algorithms, not how they are called, and fixing them helps every build,
  `-O` or not: see [The runtime](#the-runtime).

### What the own-code gap is made of

zap-basic's 44.2M, by instruction class:

| class | acc | agondev | excess |
|---|---:|---:|---:|
| loads from locals `(ix-d)` | 17.56M | 5.88M | 11.67M |
| loads from parameters `(ix+d)` | 13.61M | 3.97M | 9.63M |
| stores to locals and parameters | | | 3.71M |
| push/pop, arguments and saves | 8.37M | 2.40M | 5.98M |
| widening bytes (`or a; sbc hl,hl`) | 5.85M | 1.26M | 4.58M |
| call/ret, frameset | | | 4.70M |
| jumps, register moves, the rest | | | ~8M |

A quarter of acc's time in zap is reading `(ix+d)`. agondev keeps
`assemble_line`'s `p`, `e` and `stop` in registers and expands
`parse_operand`, an `always_inline` helper, into it. acc reads each
parameter from the frame at almost every use, and calls `parse_operand`
(15.3M on its own) with a frame of its own.

The small programs show the same things more plainly:

- **sieve**: the whole 4.8M gap is loop variables in the frame. The
  `j += i` loop is 53 cycles a turn against 30: acc loads `j` three times
  and stores it once, and loads `i` too, where agondev keeps `j` in IY and
  `i` in DE. The clearing loop is 47 against 17: agondev walks a pointer.
- **matmul**: in the inner loop, about 140 cycles a turn go on rebuilding
  `i*60+base`, `k*3` and `k*60` each time, where agondev hoists the row
  address and steps pointers (`add iy,de`, `lea iy,iy+60`).
- **lists**: `*head` is loaded three times per turn (33 cycles against
  11), with no reuse of a value already read.
- **fp**: agondev computes `zr*zr` once for the loop test and the body;
  acc calls the float multiply four times per turn where agondev calls it
  three, 13M.
- **pearson8** in zap: 91 cycles a turn against 32. agondev holds `k` in
  C, `h` in E, `p` in HL and the table in IY, with no frame at all.

## What `-O` would do, and what each is worth

Ranked by what it recovers. The zap figures are out of its 44.2M gap.

1. **Register allocation over a whole function.** Locals and parameters
   whose address is never taken live in registers across statements and
   loops. On zap it recovers about 17 to 20M (three quarters of the excess
   frame traffic); the hand-placed IY local alone took 4.3M. On sieve it
   is the whole 4.8M, and on matmul and lists 40-50% of the loop gap. It
   is also what everything below needs in order to pay: inlining alone
   measured 0.13% on zap when it was tried and dropped, because the
   inlined body still went through the frame.
2. **Inlining**: `always_inline`, `static inline` and single-caller
   statics expanded into their callers, with frameless leaves and a
   written-out prologue. About 6 to 7M on zap: frameset's 2.9M, call and
   ret's 1.8M, and part of the argument pushes.
3. **Strength reduction and loop-invariant hoisting.** Index scaling
   becomes a pointer step, and an invariant address is computed once.
   About 140 cycles per inner turn in matmul, plus sieve's clearing loop
   and sort's partition loop, which is 72 cycles a turn where a pointer
   walk would be about 20.
4. **Reusing computed values**: loads (`*head`), address arithmetic and
   the pure runtime calls (float multiply). lists' main loop; 13 to 26M of
   fp.
5. **Keeping narrow values narrow**: bytes held in A, C or E rather than
   widened through `sbc hl,hl` at every use. About 3.5M on zap.
6. **Knowing an induction variable's range**, so `k < N` for a `k` that
   starts at 0 and only grows is an unsigned compare: 8 cycles on every
   counted loop's test.
7. **Instruction shapes**: `ld de,(nn)` straight from a global, no
   pointless `push de` around a scaling, `|` and `^` of two registers a
   byte at a time. About 2 to 5M on zap.

With 1 to 5, zap-basic comes out at about 1.10 to 1.15 of agondev. On
sieve, matmul, sort and lists there is reason to expect better than
agondev, because agondev has weaknesses of its own:

- it scales every index into an array of 3-byte values with a call to
  `__imulu` (1.9M in sort);
- it moves values between registers through push/pop copies;
- it spills badly in nested loops;
- it makes each signed compare a `__setflag` call.

acc's own code already beats it in sort's loops, and on crc, interp,
wide64 and words.

## How: a second backend that sees the whole function

Everything above needs what acc lacks: a function held as a whole, so that
decisions can be made about all of it before any code exists. The design
this plan proposes gets that **without touching the parser**.

The parser drives code generation through the interface in `gen.h`: a
stack machine of about 60 operations (`vpush_local`, `vapply`, `vderef`,
`vstore_indirect`, `gen_call`, `gen_jump_if_false`, `gen_label`, ...).
stmt.c, expr.c and decl.c call `out_` directly only four times between
them. So under `-O`:

1. **Record.** Every call through the interface is also written to a log
   for the function, with its arguments: a linear stack-machine
   description of the function. The existing backend still runs as
   today, so it still answers every question the parser asks (`vtype`,
   `vconst_top`, the marks) exactly as it does now. The parser's
   rollbacks (`gen_rollback`) truncate the log with the code. Text the
   parser replays (a `for` loop's step, an inline expansion) is just more
   operations in the log.
2. **Build.** At `gen_func_end`, the log becomes an intermediate form:
   basic blocks of three-address operations on virtual registers, in SSA.
   Every local whose address is never taken becomes a set of virtual
   registers; the others stay memory.
3. **Optimise.** Inlining, copy and constant propagation, reusing values,
   hoisting, strength reduction, dead code, range analysis.
4. **Allocate and select.** A register allocator that knows the eZ80:
   HL, DE, BC and IY, with A and the byte halves for narrow values, and
   IX the frame. There is no 24-bit `ld rr,rr`, so moving a value between
   registers costs a push/pop or an `ex`, and `lea rr,iy+d` is the cheap
   way out of IY. A call clobbers everything but IX and IY. Instructions
   come from the emitters in insn.c, arith.c and wide.c that the classic
   backend already has.
5. **Keep the better one.** Both bodies exist at the end: the classic
   one and the optimised one. Keep the smaller one, or the faster one by
   a loop-weighted count of cycles, and never lose to today. A function
   that uses something the new backend cannot do yet (VLAs, `setjmp`,
   `long long`, floats, at first) keeps the classic body. The new backend
   can therefore grow one construct at a time, and be correct on every
   program from its first day.

### Why not the alternatives

- **An optimiser over the emitted machine code.** Promoting frame slots
  to registers after the fact was tried, as a cache of one local in IY:
  correct, but it recovered 0.6% on zap. Every loop
  head is a join, so the cache reloaded each turn, and code already
  shaped for the frame cannot be reshaped for registers.
- **Rewriting the parser to build a syntax tree.** It would cost a
  rewrite of the front end (expr.c and stmt.c are 5,000 lines) for what
  the log gives for free. It would also give up the one front end that
  has passed the conformance suites.
- **Compiling each function several ways and keeping the best.** This
  could choose the IY local, or whether to inline, but nothing it chooses
  between keeps values in registers. It is a useful step 0 (below), and
  no more than that.

## The runtime

Separate from `-O`, and worth doing first, because every build gains:

| routine | acc | libagon | where it shows |
|---|---:|---:|---|
| float multiply | ~1,050-1,155 cycles | 622 | fp, 49M of its 68M gap together with the next three |
| float add | ~713-750 | ~350 | fp |
| float compare | 117 | 53 | fp |
| float divide | 2,079 | 1,014 | fp |
| int to float | 643 | 406 | fp |
| 24-bit multiply | ~163 | ~111 | matmul, 0.8M |
| long multiply | ~480 | ~164 | sort 1M, lists and matmul 0.2M each |
| long add | | | lists, 5 times libagon's |

libagon keeps a float unpacked in registers between steps. acc's routines
unpack and pack through memory. Rewriting fadd and fmul, and the
unpacking and packing they share, alone would take fp from 2.09 to about
1.3.

## Milestones

Each ends measured: `test/perf.sh` and `test/size.sh` with and without
`-O`, and the whole test suite with `-O`.

0. **`-O` itself, with what already exists.**
   - The flag, passed down to the compile. It is host-first, and whether
     it goes into acc.bin or a separate Agon binary is decided at step 2.
   - With it: the pre-scan that picks the IY local (branch `iy-prescan2`:
     zap -3.8%, lists -11.7%, sieve -7.4%, but +10% compile time, which no
     longer matters), the written-out prologue (zap -2.2%, words -3.5%),
     and frameless leaves.
   - `test/run.sh` and the torture runs gain an `-O` pass.
1. **The log, replayed.** `-O` records each function and generates it by
   replaying the log into the classic backend. The output has to be
   identical, byte for byte, over the whole test suite: the proof that the
   log holds everything.
2. **The intermediate form and a plain backend.** Built from the log for
   int, pointer and byte values, with no optimisation yet and the classic
   body as the fallback. The measure is correctness: every case, the
   torture sweep and csmith, with `-O`.
3. **Register allocation.** The step the others are worth nothing without.
   Target: sieve and lists at or under agondev, zap-basic under 1.35.
4. **Inlining, value reuse, hoisting, strength reduction, narrow values,
   ranges**, one at a time, each kept only if the corpora say so. Target:
   zap-basic about 1.10-1.15; the integer programs under 1.00.
5. **`long`, `long long` and float in the intermediate form**, with a
   register convention for float calls.

The runtime work runs alongside all of it.

## What `-O` costs

- **Memory.** The log and the intermediate form hold one function at a
  time. On the host that is nothing. On the Agon, a function of a few
  hundred statements needs a few tens of KB. The heap is 188 KB, and
  acc's own sources are the largest input it compiles.
- **acc.bin.** A second backend is thousands of lines. Every byte of it
  in acc.bin is a byte of heap lost, `-O` or not. So on the Agon it
  should be its own binary, `acco.bin`, unless it turns out small.
- **Compile time on the Agon.** Several times today's. Accepted: it is
  what `-O` is for.
- **Testing.** Two backends, each tested on everything. Keeping the
  better body means a bug in the new one can hide behind the old one's,
  so step 2's correctness runs force the new backend wherever it can
  compile a function.

