# A C99 conformance suite for acc: the plan

acc is written towards C99, and today nothing measures how far it has got.
The tests in `test/cases` check what their author thought to check, one
feature at a time; `test/cpp89` measures the preprocessor against a real
conformance suite and nothing measures the rest of the language; and
`test/fuzz.sh` checks random programs over a narrow slice. None of them
answers "which parts of C99 does acc accept and get right". This is the
plan for a suite that can, built from tests other compilers already have,
rather than written from nothing.

## What acc can run today

This section used to be a list of what acc could not do, and it shaped the
whole plan. Most of it has since been built. What follows was checked
against the compiler rather than remembered:

- **A preprocessor.** `#include`, `#define` with parameters and `...`,
  `#if`/`#ifdef`/`#elif`, `#line`, `#error`, `#pragma once`, `#` and `##`,
  and argument prescan. 139 of Decus CPP's C89 conformance tests pass; see
  `test/cpp89`.
- **A library.** Twelve headers -- `<assert.h>`, `<ctype.h>`, `<limits.h>`,
  `<math.h>`, `<stdarg.h>`, `<stdbool.h>`, `<stddef.h>`, `<stdint.h>`,
  `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<time.h>` -- over 28 members,
  linked from `bin/libc.a` by acc's own linker, which takes only what is
  used. `<math.h>` is the whole of 7.12 that one floating type can carry,
  each function measured against the host's library rather than assumed:
  exact for the square root, within an ulp or two for most of the rest.
- **The language.** Structs, unions, enums, bit-fields (including of
  `long long`, to 64 bits), `typedef`, `sizeof`, `switch`, `do`, `break`,
  `continue`, string and character literals, `void` functions, designated
  initialisers, compound literals, variable-length arrays, flexible array
  members, `long long`, `_Bool`, `inline`, `restrict`, `__func__`, the
  forty macros that say how wide everything is, and `-D`/`-U` on the
  command line. Of 36 C99 feature probes acc takes 34.
- **No ceiling on the size of a function.** (ix+d) reaches 128 bytes, and a
  function that wanted more used to be refused. What it declares past the
  window now lives where the arrays do, and the scratch past it is reached
  through IY. Nothing in the torture suite is refused for its frame.

What is still missing, and what each one costs, is the first thing the
measurements below say.

Two things have not changed, and they still shape everything:

- **agondev's data model.** `int` is 24 bits, `long` 32, and `double` is
  the same 4-byte float as `float`. Tests that assume a 32-bit `int` or a
  64-bit `double` are not wrong, but they are not tests of this machine.
- **One byte of output.** A program reports by returning from `main`: `-x`
  sends the low byte to IO port 0, which the emulator turns into its exit
  status. acc can print now, so a test that compares stdout is possible,
  but the exit byte is still the cheapest and most of the tests worth
  having use it.

## What the dry run said, and where it stands

The filter this plan settles on below was run over the largest candidate
source before any of this was built, to size the work, and has been re-run
after each piece of it. `gcc.c-torture/execute`, 1,698 files:

| | tests |
|---|---|
| in the directory | 1,698 |
| strict C99 for this target, under agondev | **1,303** |
| acc compiled when this plan was first written | 1,049 (80.5%) |
| acc compiles now | **1,173** (90.0%) |
| acc refuses | 130 |

What moved it, measured one piece at a time rather than attributed after
the fact:

| what was built | tests it unblocked |
|---|---|
| `abort`, `-D`/`-U`, and the forty macros that say how wide everything is | +98 |
| bit-fields of `long long`, to 64 bits | +8 |
| `va_arg` and the rest through a `va_list *` | +4 |
| `&` through a cast -- `&((struct R *) q)->a`, and offsetof's old spelling | +3 |
| `<ctype.h>` and `<assert.h>` | +2 |
| `<math.h>` | +2 |
| the frame-size ceiling, taken off | +6 |
| `&` of a string literal, and of a byte inside one | +1 |

What is left, by what it actually needs -- which is not the same as what
its first error message says, and the difference is the subject of a risk
below:

| what it needs | tests |
|---|---|
| GNU extensions: keywords, attributes, builtins, `asm`, statement expressions | 36 |
| old-style (K&R) function definitions | 27 |
| `long double` | 26 |
| wide literals -- `L"..."` and `L'x'` | 8 |
| `setjmp.h` | 1 |
| `_Complex` | 1 |
| everything else, in ones and twos | 31 |

Three of those are settled and will not move: GNU extensions are out of
scope, K&R definitions are out by decision (nobody should be writing them
for this machine, and supporting them would shape the parser around a form
C99 calls obsolescent), and `long double` is out because agondev's library
has no arithmetic for one -- so a program that asks for it does not link,
whichever compiler built it. That is 89 of the 130, and the honest ceiling
on this suite is therefore about 1,214 of 1,303 rather than all of them.

The remaining 41 are worth reading one by one. The C99 gaps in them that
have been identified so far:

- **`[static n]` in an array parameter** -- `void f(int a[static 4])`,
  which is 6.7.5.3p7 and not an extension. 2 tests.
- **Wide literals.** 6.4.4.4 and 6.4.5. `wchar_t` is `short` here, to match
  agondev. 8 tests.
- **Constant expressions at the edges**: a global initialised with
  `&"X"[0]`, an enum constant that needs a cast folded, an array size that
  does too, and an address constant with a `+` in it. 4 tests, one each.
- **`setjmp.h`**, which is a library gap and not a language one. 1 test.

And four of the 41 are not acc gaps at all: `strlen-2`, `strlen-3`,
`strlen-4` and `memchr-1` fail on `__builtin_printf`, and the `?:` with a
void side that their error message names works correctly.

## What "compiles with C99" has to mean

Every test that compiles as C99 belongs in the suite, whatever it is named
and whatever it was written for. `gcc.dg/c99-*` is 135 files out of 7,562
in that one directory; the other 7,427 are not excluded by their names.
The filter decides membership, and nothing else does.

Which makes the filter's flags the most consequential decision here, and
they are easy to get wrong in the strict direction. Run over the torture
tests with `-std=c99 -pedantic-errors` alone, the filter accepts 583 files.
With the flags below it accepts 1,303. The 720 in between are almost all
old-style function declarators -- which C99 calls obsolescent, in 6.11.6
and 6.11.7, and obsolescent is not the same as invalid. A filter that
rejects them is not measuring C99; it is measuring a house style.

So the rule is: what C99 permits is in, including what it deprecates.
What C99 removed is out. Concretely, from the same run:

- **In:** old-style definitions, `()` declarators, and everything else
  6.11 calls obsolescent. `-Wno-deprecated-non-prototype`,
  `-Wno-strict-prototypes`.
- **Out:** implicit `int` (90 tests), which C99 removed; implicit function
  declarations (14), same; nested functions (23), statement expressions,
  address-of-label, empty structs, zero-length arrays, empty initialisers,
  complex integers, vector attributes -- GNU extensions, all named as such
  by clang and each excluded with that name recorded.

The exact flag set lives in `sources.txt` beside the pinned revision,
because it changes the answer by 720 tests and a result that cannot say
which flags produced it is not reproducible.

## The candidate sources

| source | size | how a test reports | license | fit |
|---|---|---|---|---|
| [gcc torture `execute`](https://github.com/gcc-mirror/gcc/tree/master/gcc/testsuite/gcc.c-torture/execute) | 1,698 files; **1,303 are C99 here**, acc compiles **1,173** | `abort()` on failure, `exit(0)` | GPL-3 (part of GCC) | **start here**: measured, and the only source large enough for a scoreboard to mean anything |
| [gcc `gcc.dg`](https://gcc.gnu.org/onlinedocs/gccint/C-Tests.html) | 7,562 `.c`, of which 135 are named `c99-*` | DejaGnu: `dg-do run`, or `dg-do compile` with `dg-error` lines | GPL-3 | the `c99-*` ones name their clause; the other 7,427 go through the filter like everything else |
| [c-testsuite](https://github.com/c-testsuite/c-testsuite) `single-exec` | 220 tests | `main` returns 0, plus a `.expected` stdout file | framework MIT; tests carry their own, per `.otags` | all 220 now: the cpp/libc split it used to be cut on has no meaning |
| ↳ from [scc](https://www.simple-cc.org/) | 150 of the 220 | returns 0 | ISC | permissive, so it can be vendored |
| ↳ from [tinycc](https://bellard.org/tcc/) `tests2` | 69 of the 220 | stdout | LGPL | compatible with acc's own licence |
| [chibicc `test/`](https://github.com/rui314/chibicc) | ~40 files, dense | `ASSERT(expected, expr)` via `test.h` | MIT | the best coverage per line here, and nothing blocks it any more |
| [tinycc `tests2`](https://github.com/TinyCC/tinycc/tree/mob/tests/tests2) | 139 `.c`, 137 `.expect` | stdout compared with `.expect` | LGPL | some GNU attributes, which the filter takes out |
| [llvm-test-suite](https://llvm.org/docs/TestingGuide.html) `SingleSource` | hundreds | stdout vs a reference output | Apache-2.0 with LLVM exception (check per directory) | mostly benchmarks, some regressions; the one source with files big enough to need reducing |
| Csmith / YARPGen | unlimited | random program prints a checksum | BSD / Apache | extends `test/fuzz.sh` rather than the suite |
| [Plum Hall](https://plumhall.com/newsite/suites.html), Perennial, SuperTest | thousands, clause-indexed | own harness | commercial | the real thing, if it is ever worth paying for |

There is no open, clause-by-clause C99 validation suite; the commercial ones
are the only complete ones. The open sources above overlap and complement
each other, and together they cover most of the language.

## The key idea: let agondev decide what is a C99 test

Most of the candidates are not strictly C99: gcc's torture tests use GNU
extensions, and tinycc's use attributes. Sorting 2,000 files by hand is not
worth doing. agondev can do it:

1. **Strict C99.** Compile with agondev's clang at the flags the section
   above settles on -- `-std=c99 -pedantic-errors`, with
   `-Werror=implicit-function-declaration` for what C99 removed and
   `-Wno-deprecated-non-prototype -Wno-strict-prototypes` for what it only
   deprecates. A test that does not compile is not a C99 test, and is
   excluded with the reason clang gave, which names the extension.
2. **Correct on this machine.** Run agondev's build on the emulator. A test
   that fails there assumes something this data model does not give it, such
   as a 32-bit `int`, a 64-bit `double` or a libc function. It is excluded
   too, with the reason recorded.
3. **What is left** is strict C99 that agondev compiles correctly for the
   Agon, and its answer on the emulator is the reference. That is the same
   differential test that `test/run.sh` and `test/fuzz.sh` already use.

The filter runs once per import and its results are recorded. It does not
run every time the suite does.

## When a test cannot run, keep what it was testing

A test acc cannot run is not nothing. It was written because some part of
C99 was got wrong once, and what it covers is lost the moment it is struck
off a list. Counting exclusions says how much was lost and never what, so
the scoreboard would read 1,173 of 1,303 and not say which parts of the
language the other 130 were the only cover for.

So every test carries a **census** of what it is made of, taken at import
whether or not it can run, and kept in the manifest beside it.

The census is mechanical. Read the file and record the C99 constructs in
it: the keywords, the operator forms, the declarator shapes, the
initialiser forms, the storage classes, the types. `test/bench.sh` already
does a crude version of this -- it greps its inputs and reports which
keywords and operators no input uses -- and the same walk, with the
comments stripped the same way, is what tags a test. Each construct maps
to the clause it belongs to: 6.4 lexical, 6.5 expressions, 6.7
declarations, 6.7.8 initialisation, 6.8 statements, 6.9 external
definitions, 6.10 preprocessing, 7 library.

What that buys is a scoreboard by clause rather than by count:

```
6.7.8 initialisation      41 tests   34 pass   5 need <abort>   2 excluded
6.7.2.1 bit-fields        18 tests    6 pass  12 need <wide bit-fields>
```

A clause with no cover at all reads as a hole, and a clause whose only
cover is a test that cannot run reads as a hole too, which is the case
that a bare count hides.

**Where a test is blocked by size rather than by a feature, reduce it.**
Nothing in the torture tests is near acc's limit, but other sources have
generated files that are, and acc's limit is not the file: it is the heap
the compile runs in, which grows with how much of the program is live at
once and not with how many bytes went past. A test that will not fit is
shrunk under a predicate that keeps three things true:

1. agondev still compiles it under the C99 flags,
2. it still answers what it answered before, and
3. **its census is unchanged** -- the constructs that made it worth having
   are all still in it.

`test/fuzz/reduce.py` already reduces a program against a predicate and is
where this starts. The reduced file is committed beside the manifest row
that points at the original and at the revision it came from, so what is
being run can always be read against what it was cut down from.

Three ways it can end, and each is recorded as itself:

- **Reduced.** The smaller test runs and its census is the original's. The
  coverage is kept and the row says it was reduced.
- **Reduced, and something was lost.** No reduction keeps the whole census.
  Commit the largest one that fits, and record the constructs that fell
  out, so the clause they belong to shows as partly covered rather than
  covered.
- **Not reducible.** The size is the thing under test -- a switch with
  thousands of cases, an initialiser with thousands of elements. Then the
  finding is about acc: it cannot compile a unit of that shape at that
  size. Record the size it failed at, which makes it a number that can
  move, and leave the clause marked as uncovered.

The same treatment goes to a test excluded because agondev gets it wrong.
Its census is recorded all the same, so the coverage lost to the reference
compiler is as visible as the coverage lost to acc.

## How the suite would be built

**Vendor what acc's license allows, and fetch the rest.** acc is LGPL-2.1 or
later. scc's tests (ISC), chibicc's (MIT) and tinycc's (LGPL-2.1 or later)
can live in the tree with their notices kept. gcc's are GPL-3, and are fetched
rather than copied, so that nothing in the repository is under terms the
compiler is not. And whatever is vendored, the imports are large. So:

- `test/conformance/sources.txt` names each upstream source with a **pinned
  revision**, so a result is reproducible.
- An importer fetches the pinned revisions into a gitignored cache and runs
  the agondev filter above.
- `test/conformance/manifest.tsv` is committed. It has one row per test:
  source path, the revision it came from, its census, the clauses that
  census maps to, what it needs to run, and a status.
  - Census: the constructs found in the file, as the section above takes
    them. It is recorded for every test, including the ones that cannot
    run, because it is what says what an exclusion costs.
  - Clauses: 6.4 lexical, 6.5 expressions, 6.7 declarations, 6.7.8
    initialisation, 6.8 statements, 6.9 external definitions, 6.10
    preprocessing, 7 library.
  - Needs: `abort`, `printf`, `k&r-definitions`, `wide-bit-fields` and so
    on -- named after the thing acc is missing, not after a vague area, so
    that building one moves a known set of rows.
  - Status: `pass`, `fail`, `needs <feature>`, `reduced`, `reduced-partial`,
    `too-big <bytes>`, or `excluded <reason>`.
- The tests that may be vendored (ISC, MIT, LGPL) can be copied in with
  their notices once the importer has decided which ones survive, if having
  them offline matters. The pinned fetch stays the source of truth.

**Adapters.** Far fewer than this plan once needed. A test is compiled
with `-I` at acc's own headers and linked against `bin/libc.a`, which is
how any other program is built, and most of them then run as they are:

- **Report by exit byte.** A test that returns 0 or calls `exit(0)` needs
  a status only, and that is what `-x` gives.
- **`abort`.** The one thing in the library that 891 of these tests want.
  Built: `exit` with a status of 134, which is what a shell reports for a
  program a SIGABRT ended, and its own library member so that a program
  which never calls it carries neither it nor the printf under it.
- **Output tests** (`printf` against an expected file) work now: acc
  prints, and the emulator harness already captures the console -- it is
  what `test/mos.sh` and `test/args.sh` read. They are still worth less
  than the exit-byte ones, because a byte is enough for a test that
  compares against a reference answer.
- **Diagnostics tests** (`gcc.dg` `dg-error` lines) become "must be
  rejected at line N" checks, in the shape `test/errors.sh` already uses.
  The line is checked and the wording is not, since acc's messages are its
  own.
- **`-D` on the command line.** Several upstream harnesses pass one.
  Built, both of them, rather than having the importer edit the file: it
  was smaller than it sounded and it does not alter what is being run.

**The runner** is `test/conformance.sh`. It works from the manifest and
reuses the emulator harness. Every test that can run goes on one SD card
with one autoexec, as `test/target.sh` does, so a few hundred tests take
seconds rather than minutes.

It reports the scoreboard the census section describes: per clause, how
many tests pass, fail, are waiting on a named feature, were reduced to
fit, or are excluded -- and for each clause with nothing passing, what was
lost.

It is strict both ways:
- A `pass` test that fails is a regression, and fails the run.
- A `needs <feature>` test that starts passing also fails the run, until the
  manifest is updated. That keeps the scoreboard honest, and makes landing a
  feature show up as tests moving to `pass`.

## The order to do it in

The order the plan used to give was set by what acc could not do, and it
put the machinery on the smallest source it could find. The measurements
said to start on the largest instead, and the first item on that list has
since been built along with several others -- so what follows is what is
done, and then what is next.

**Done, and measured:**

1. **`abort`.** In the library, as its own member, so a program that never
   calls it carries neither it nor the printf under it. 891 of the 1,303
   torture tests end in one.
2. **`-D` and `-U`**, which several upstream harnesses pass.
3. **The macros that say how wide everything is.** `__SIZE_TYPE__` and 39
   others. Not C, and no program that wants an integer of a named width
   can do without them: this was the single largest unblocker measured, at
   98 tests, and it had been hidden behind an error message that named
   something else.
4. **The language gaps the filter found**, each a few tests: `long long`
   bit-fields, `va_list` through a pointer, `&` through a cast and of a
   string literal, and the frame-size ceiling.
5. **`<ctype.h>`, `<assert.h>`, `<math.h>`.**

**Next, in this order:**

1. **gcc.c-torture/execute, all of it that the filter takes.**
   - Build the importer, filter, census, manifest, runner and scoreboard
     around it. The filter and the compile pass already exist as a script;
     what is missing is everything that turns a count into a scoreboard.
   - Fetched and not vendored: it is GPL-3 and acc is LGPL-2.1.
   - Expect 1,173 compiling on the first run, and the scoreboard to say
     which clauses the other 130 were covering.
   - It should also say what `abort` and `exit` alone cannot: how many of
     the 1,173 *run* and give the right answer. Compiling is not passing,
     and that number has never been measured.
2. **c-testsuite, all 220.**
   - ISC and MIT, so it can be vendored with its notices.
3. **The rest of `gcc.dg`, not just `c99-*`.**
   - 7,562 files of which 135 are named for C99. The filter decides.
   - The `dg-error` ones become must-reject checks. These are the tests
     that say acc refuses what C99 forbids, which nothing above does.
4. **chibicc's tests.** MIT, about 40 files, denser than anything else here.
5. **Random differential beyond `test/fuzz.sh`.** Csmith or YARPGen, with
   `test/fuzz/reduce.py` as the reducer.

**Not being done, and why:**

- **Old-style (K&R) function definitions.** 27 tests. Obsolescent in C99
  6.11.7, and nobody should be writing them for this machine; supporting
  them would shape the declarator parser around a form that is on its way
  out. The plan used to call this the single biggest language gap at 102
  tests, which was an artefact of counting by error message.
- **GNU extensions.** 36 tests. Statement expressions, nested functions,
  `__typeof__`, `__builtin_*` beyond what acc already has, attributes acc
  does not take, `asm`.
- **Computed goto.** `&&label` and `goto *p` were built and then taken back
  out: agondev's backend cannot compile a label's address at all -- "unable
  to legalize instruction: G_BLOCK_ADDR", at every optimisation level and
  under `-std=gnu99` -- so a program using it cannot be built by both
  compilers, and the case for it could only be held to coming out at 42 on
  its own rather than to agreeing with the reference build. It comes back
  if agondev ever takes it. The commit that removed it says how it was done.
- **`long double`.** 26 tests. agondev's library has no arithmetic for one.
- **A commercial suite.** Out of scope.

## Risks, and what to watch

- **The filter's flags decide the answer.** Measured: 583 tests with
  `-pedantic-errors` alone against 1,303 with the flags this plan settles
  on. A filter tightened by one flag silently stops measuring a fifth of
  C99. The flags are pinned beside the revision and a change to them is a
  change to every result.
- **Filtering by agondev inherits its bugs.** If agondev gets a test
  wrong, the test is excluded, so it silently stops testing acc. Every
  exclusion is recorded with its reason and its census, and the
  `excluded: wrong under agondev` list is read by hand. The fuzzer has
  already seen agondev hang on valid programs.
- **The data model.** Tests assuming a 32-bit `int` or a 64-bit `double`
  are excluded, not "fixed". The coverage this loses is real, and the
  census is what measures it.
- **Size is not the worry it was.** The torture tests have a median of 394
  bytes and a largest of 12.8 KB. Other sources have generated files that
  are larger, and what they run into is the heap rather than the file, so
  the reduction path exists -- but nothing measured so far needs it.
- **Clause mapping is judgement.** gcc's c99 tests name their clause.
  Everything else is assigned from its census, which only has to be good
  enough to point at the right chapter.
- **An error message is not a cause, and counting by message lies.** This
  plan's first table read the compiler's first error for each refused test
  and grouped by it. `expected a type, found a name` came to 102 tests and
  was written down as old-style function definitions, which made them the
  largest gap and the third item on the build order. The real figure was
  27. The same message came from `__inline__`, and from every test that
  declared a variable of a type acc had not defined a macro for -- which
  was 98 tests, the largest gap in the suite, and it was invisible because
  it wore another gap's message. Nothing since has been counted that way:
  a group is read by what its tests are made of, and the number is checked
  by building the thing and re-running the filter.
- **Compiling is not passing.** 1,173 of 1,303 is how many acc *compiles*.
  How many run and give the right answer is a different number and has not
  been measured; the two that have been spot-checked by hand both passed,
  which is not evidence. The scoreboard has to report both or it is
  measuring the parser and calling it conformance.
- **A scoreboard invites a number.** 1,173 of 1,303 is not "90% C99
  conformant"; it is how many of one suite's tests compile. The clause
  rows are the answer and the total is a headline. And 89 of the 130 left
  are settled as out of scope, so the ceiling on this suite is about 1,214
  -- a scoreboard that reads 1,173 of 1,303 without saying so implies 130
  tests of work that does not exist.

## Decisions, and what was decided

The three this plan opened with are settled:

1. **`abort` goes in the library**, not in a test prelude -- a C99 function
   acc ought to have anyway, and its own member so that a program which
   never calls it carries neither it nor the printf under it.
2. **`-D` and `-U` were built**, rather than having the importer edit the
   tests. Smaller than it sounded, and it does not alter what is being run.
3. **No commercial suite.** Out of scope.

And three more have been made since, each of which takes tests off the
board for good rather than putting them on it:

4. **No old-style (K&R) function definitions.** 27 tests.
5. **No `long double`.** 26 tests, and not really acc's decision: agondev's
   library has no arithmetic for one.
6. **No GNU extensions**, and computed goto was taken back out after being
   built, because agondev cannot compile it and a test acc alone can run is
   worth much less than one both compilers answer.

What is still open is the same thing that was open at the start, and it is
now the only thing between this plan and a scoreboard: **none of the
machinery exists yet.** The filter, the compile pass and the count are a
shell script and an awk line. The census, the manifest, the clause mapping,
the runner and the scoreboard are all still to build, and until they are,
what this document reports is a number and not a measurement of C99.
