# A C99 conformance suite for acc: the plan

acc is written towards C99, and today nothing measures how far it has got.
The tests in `test/cases` check what their author thought to check, one
feature at a time, and `test/fuzz.sh` checks random programs over a narrow
slice of the language. Neither can answer "which parts of C99 does acc
accept and get right". This is the plan for a suite that can, built from
tests other compilers already have, rather than written from nothing.

No code yet: this is what to build, from what, and in what order.

## What acc can run today, which shapes everything

- **No preprocessor.** No `#include`, `#define` or `#if`. Most published tests
  start with `#include`.
- **No C library.** No `printf`, `abort`, `exit`, `memcpy` or `strlen`. Many
  tests check their result by printing it, or by calling `abort()`.
- **One byte of output.** A program reports by returning from `main`: `-x`
  sends the low byte to IO port 0, which the emulator turns into its exit
  status.
- **agondev's data model.** `int` is 24 bits, `long` 32, and `double` is the
  same 4-byte float as `float`. Tests that assume a 32-bit `int` or a 64-bit
  `double` are not wrong, but they are not tests of this machine.
- **Language gaps.** There are no structs, unions or enums, no `typedef`,
  `sizeof`, `switch`, `do`, `break`, `continue`, string or character
  literals, or `void` functions.

So a suite built today mostly records what fails. Its value is in saying
exactly which clause each failure belongs to, and then telling us when a new
feature makes a test pass.

## The candidate sources

| source | size | how a test reports | license | fit |
|---|---|---|---|---|
| [c-testsuite](https://github.com/c-testsuite/c-testsuite) `single-exec` | 220 tests; 121 need neither cpp nor libc | `main` returns 0, plus a `.expected` stdout file | framework MIT; tests carry their own, per `.otags` | **best first source** |
| ↳ from [scc](https://www.simple-cc.org/) | 150 of the 220 | returns 0 | ISC | permissive |
| ↳ from [tinycc](https://bellard.org/tcc/) `tests2` | 69 of the 220 | stdout | LGPL | copyleft |
| [gcc torture `execute`](https://github.com/gcc-mirror/gcc/tree/master/gcc/testsuite/gcc.c-torture/execute) | 1,703 files | `abort()` on failure, `exit(0)` | GPL-3 (part of GCC) | large; many GNU extensions and K&R |
| [gcc `gcc.dg/c99-*`](https://gcc.gnu.org/onlinedocs/gccint/C-Tests.html) | 136 files | DejaGnu: `dg-do run` or `dg-do compile` with `dg-error` lines | GPL-3 | **the only set organised by C99 clause**; mostly needs cpp |
| [tinycc `tests2`](https://github.com/TinyCC/tinycc/tree/mob/tests/tests2) | 139 `.c`, 137 `.expect` | stdout compared with `.expect` | LGPL | needs `printf`; some GNU attributes |
| [chibicc `test/`](https://github.com/rui314/chibicc) | ~40 files, dense | `ASSERT(expected, expr)` via `test.h` | MIT | needs cpp and `printf`; very high coverage per line |
| [llvm-test-suite](https://llvm.org/docs/TestingGuide.html) `SingleSource` | hundreds | stdout vs a reference output | Apache-2.0 with LLVM exception (check per directory) | needs `printf`; mostly benchmarks, some regressions |
| Csmith / YARPGen | unlimited | random program prints a checksum | BSD / Apache | needs structs and `printf`; extends `test/fuzz.sh` later |
| [Plum Hall](https://plumhall.com/newsite/suites.html), Perennial, SuperTest | thousands, clause-indexed | own harness | commercial | the real thing, if it is ever worth paying for |

There is no open, clause-by-clause C99 validation suite; the commercial ones
are the only complete ones. The open sources above overlap and complement
each other, and together they cover most of the language.

## The key idea: let agondev decide what is a C99 test

Most of the candidates are not strictly C99: gcc's torture tests use GNU
extensions, and tinycc's use attributes. Sorting 2,000 files by hand is not
worth doing. agondev can do it:

1. **Strict C99.** Compile with agondev's clang
   `-std=c99 -pedantic-errors -Werror=implicit-function-declaration`. A test
   that does not compile is not a C99 test, and is excluded with the reason
   recorded.
2. **Correct on this machine.** Run agondev's build on the emulator. A test
   that fails there assumes something this data model does not give it, such
   as a 32-bit `int`, a 64-bit `double` or a libc function. It is excluded
   too, with the reason recorded.
3. **What is left** is strict C99 that agondev compiles correctly for the
   Agon, and its answer on the emulator is the reference. That is the same
   differential test that `test/run.sh` and `test/fuzz.sh` already use.

The filter runs once per import and its results are recorded. It does not
run every time the suite does.

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
  source path, the C99 clause, the features it needs, and a status.
  - Clauses: 6.4 lexical, 6.5 expressions, 6.7 declarations, 6.8 statements,
    6.9 external definitions, 6.10 preprocessing, 7 library.
  - Needs: `cpp`, `libc`, `struct`, `switch`, `string` and so on.
  - Status: `pass`, `fail`, `needs <feature>`, or `excluded <reason>`.
- The tests that may be vendored (ISC, MIT, LGPL) can be copied in with
  their notices once the importer has decided which ones survive, if having
  them offline matters. The pinned fetch stays the source of truth.

**Adapters, so that tests written for a full C implementation run on acc:**

- **Report by exit byte.** Tests that return 0 or call `exit(0)` need a
  status only. That already works.
- **A tiny prelude instead of a libc.** `abort`, `exit` and a few pure
  functions (`memcpy`, `memset`, `strlen`, `strcmp`) can be written in the
  C acc compiles and prepended to a test, since without a preprocessor
  there is no `#include`. `abort` and `exit` need one hook in the startup
  stub: returning straight to it from any depth.
- **`#include` lines** of standard headers the test does not really need
  are stripped by the importer, and the stripping is recorded. Tests that
  need real preprocessing stay marked `needs cpp` until acc has one.
- **Output tests** (`printf` against an expected file) wait for a way to
  print. The emulator already shows console output, and MOS prints a
  character with one `rst.lil 0x10`, so a `putchar` in the prelude and a
  harness that captures the console would do. A real `printf` comes with
  libagon.
- **Diagnostics tests** (`gcc.dg` `dg-error` lines) become "must be
  rejected at line N" checks, in the shape `test/errors.sh` already uses.
  The line is checked and the wording is not, since acc's messages are its
  own.

**The runner** is `test/conformance.sh`. It works from the manifest and
reuses the emulator harness. Every test that can run goes on one SD card
with one autoexec, as `test/target.sh` does, so a few hundred tests take
seconds rather than minutes.

It reports a scoreboard: per clause, how many tests pass, fail, are waiting
on a named feature, or are excluded.

It is strict both ways:
- A `pass` test that fails is a regression, and fails the run.
- A `needs <feature>` test that starts passing also fails the run, until the
  manifest is updated. That keeps the scoreboard honest, and makes landing a
  feature show up as tests moving to `pass`.

## The order to do it in

1. **c-testsuite, the 121 tests that need neither cpp nor libc.**
   - Build the importer, filter, manifest, runner and scoreboard around them.
   - Most will be `needs <feature>` at first; that is the point.
   - Exit-status reporting works as-is.
   - Everything later reuses this machinery.
2. **The prelude and the gcc torture tests.**
   - Add `abort`, `exit` and a few memory and string functions, and the
     startup hook.
   - Import `gcc.c-torture/execute` through the agondev filter.
   - This is the bulk: expect a few hundred strict-C99 tests to survive,
     with good coverage of expressions and conversions.
3. **Diagnostics.**
   - Turn `gcc.dg/c99-*` compile tests with `dg-error` lines into
     must-reject checks, and the `dg-do run` ones into ordinary runs.
   - These are the tests named after C99 clauses, and they check that acc
     refuses what C99 forbids, which nothing above does.
   - Many need cpp, so they arrive gradually.
4. **Output tests**, once acc can print:
   - c-testsuite's `needs-libc` tests (63), tinycc `tests2`, and chibicc's
     tests with its `ASSERT` rewritten into the prelude.
   - chibicc's are dense, and would be the best single addition once
     structs and cpp exist.
5. **Random differential beyond `test/fuzz.sh`.**
   - Csmith or YARPGen once structs and a way to print a checksum exist.
   - They find what no fixed suite does, and `test/fuzz/reduce.py`
     generalises into a reducer for them.
6. **A commercial suite**, only if acc ever needs a claim of conformance
   rather than confidence in it.

## Risks, and what to watch

- **Filtering by agondev inherits its bugs.** If agondev gets a test wrong,
  the test is excluded, so it silently stops testing acc. Record every
  exclusion with its reason, and look at the `excluded: wrong under
  agondev` list by hand. The fuzzer has already seen agondev hang on valid
  programs.
- **The data model.** Tests assuming a 32-bit `int` are excluded, not
  "fixed". The coverage this loses is real, and is measured by how many
  there are.
- **Size.** acc runs in 448 KB, and some torture tests are large
  generated files. A test acc cannot fit is a finding about acc, and is
  marked as such rather than excluded.
- **Clause mapping is judgement.** gcc's c99 tests name their clause.
  Everything else has to be assigned at import, by feature. It only needs
  to be good enough to point at the right chapter.

## Decisions for you

1. **Order.** Whether the prelude (step 2) is worth building before the
   preprocessor, which unblocks far more tests.
2. **Commercial.** Whether a paid suite is ever in scope.

acc's own license was the third, and is settled: LGPL-2.1 or later, which is
what decides the vendoring above. The prelude of step 2 is copied into test
programs the way `src/rt/` is copied into every program, so it takes the
runtime's license: LGPL-2.1 or later with glibc's linking exception.
