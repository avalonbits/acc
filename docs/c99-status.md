# acc and C99: where it stands

A snapshot, taken at 9c63361 on 2026-09-25. How it was reached, and the
reasoning behind the suite and its filter, is in
[c99-conformance-plan.md](c99-conformance-plan.md); this page is the result,
and the list of what is still open.

In short: acc compiles and runs nearly every valid C99 program the four
test suites hold, apart from what it leaves out by decision. What remains
is mostly the other direction -- invalid programs that acc accepts when C99
says to refuse them.

## The conformance suite

`test/conformance.sh` holds acc to a manifest per source,
`test/conformance/<source>.tsv`. A test counts only if agondev's clang calls
it strict C99 and agondev's own build of it gives the right answer on the
Agon; the rest are excluded, each with its reason recorded. Of what is left:

| source | tests counted | acc passes | fails | refuses |
|---|---|---|---|---|
| gcc.c-torture/execute | 1,235 | **1,129** | 6 | 100 |
| c-testsuite single-exec | 198 | **197** | 1 | 0 |
| gcc.dg (run, compile, must-refuse) | 4,516 | **3,708** | 174 | 634 |
| chibicc, an ASSERT per program | 311 | **289** | 1 | 21 |

The check is strict both ways: a test that stops passing fails
`make test`, and so does one that starts passing until the manifest is
updated.

### What the refusals are

| waiting on | torture | gcc.dg | chibicc |
|---|---|---|---|
| GNU extensions | 55 | 496 | 18 |
| old-style (K&R) definitions | 26 | 23 | |
| `long double` | 10 | 46 | |
| `_Complex` | 7 | 51 | |
| a constant initialiser C99 does not allow | | 7 | 2 |
| a call with arguments to a function defined with `()` | 1 | 3 | |
| the measuring device's builtin mapping | 1 | 1 | |
| C11's headers | | | 1 |
| not yet classified | | 7 | |

All but the last row are out of scope or out by decision (below). The seven
unclassified are in the open list.

### What the failures are

- **torture, 6.** Five use what acc does not do and C99 does not ask for:
  `aligned` (20010904-1, -2, pr23467), `mode` (cbrt) and
  `#pragma push_macro` (pushpop_macro). pr58943 depends on an evaluation
  order C leaves unspecified.
- **c-testsuite, 1.** 00206 is `#pragma push_macro` again.
- **chibicc, 1.** unicode-51 compares four bytes of `L""`, which is two
  bytes here, so it reads past the object; agondev's layout happens to
  forgive it.
- **gcc.dg, 174.**
  - 97 invalid programs acc accepts.
  - 64 it refuses, but at a line the test does not name.
  - 12 that abort: mostly GNU attributes and sibling-call tests
    (`cleanup`, `constructor`, `ms_struct`, `sibcall-*`, `-fwrapv`),
    and c99-hexfloat-2, which is a real bug (below).
  - pr103222, which crashes.

## Random programs

`test/csmith.sh` builds Csmith's programs with acc and with agondev and
compares the checksum each prints. In the last run of 200, 78 were run and
75 agreed; the other three have a source line longer than acc's 16 KB
window, past C99's 4,095-character minimum, and are skipped now. Most of
the rest were skipped because they compare pointers to types C99 does not
let them, which clang only warns about and acc refuses. `test/fuzz.sh`
covers narrow expressions the same way.

`test/yarpgen.sh` does the same with YARPGen, which writes only C that is
valid by its own model -- a 32-bit int. Of 150 programs, 107 ran and 104
agreed. The three that did not are undefined here and not in YARPGen's
model: a `long` addition that overflows once a 32-bit constant has been
put in a 24-bit int, and two shifts of an int by 25 and 27 bits. It also
found that a call could have 64 arguments pending at most, where C99
5.2.4.1 asks for 127 -- YARPGen's drivers pass 70 to 90 -- since fixed.

## Compile speed

584.8 cycles per byte of source on the Agon (`test/bench.sh` with the
cycle-counting build), against a goal of under 600. The redeclaration
checks added during the conformance work cost about 0.3%.

## Out by decision

Each is also in the plan's decisions:

- **Old-style (K&R) function definitions.** Obsolescent in C99 6.11.7.
- **`long double`.** agondev's library has no arithmetic for it.
- **`_Complex`.** Optional for a freestanding implementation.
- **GNU extensions.** `__attribute__` is read and ignored, and
  `__restrict` is read as `restrict`; nothing else.
- **Trigraphs** are read only with `-trigraphs`, as gcc and clang have
  them outside their strict modes.
- **`double` is `float`**, four bytes, to match agondev's calling
  convention. C99 5.2.4.2.2 asks for more; `<float.h>` says what is true.

## Open

### Bugs and gaps that are acc's

- **Invalid programs accepted.** 97 gcc.dg must-refuse tests compile, and
  64 more are refused at a line the test does not name. The largest open
  item, and none has been read one by one.
- **`0x1p+f` is not one preprocessing number.** C99 6.4.8 makes a
  pp-number take `p+` and `p-`, so `0x1p+f` is a single token and
  stringises as `"0x1p+f"`; acc gives something else (gcc.dg's
  c99-hexfloat-2).
- **A pointer parameter's pointee qualifiers are not compared** between
  declarations: `int f(const char *s); int f(char *s);` is taken, and C99
  6.7.5.3p15 makes the two types incompatible.
- **A VLA type name at file scope is refused** where its size is never
  evaluated: `int b = sizeof (int (*)[a]);` (gcc.dg's vla-21).
- **`((*p)) = 1`**, a dereference in two parentheses as the target of an
  assignment, is refused; `(*p) = 1` works.
- **A narrow string joined to a wide one**, when it mixes escapes that
  give bytes past 0x7f with UTF-8 written in the source, is widened a byte
  at a time: the lexer records only that such an escape was there.
- **pr103222** crashes the machine, not yet looked at.
- **Seven gcc.dg refusals not yet classified**, each of which may be a
  program acc should take:
  - c99-complit-1: an address kept in one byte.
  - lvalue-5: an assignment whose target it does not recognise.
  - pr100619: a missing dimension where it is allowed.
  - pr89211: a struct used before its members are given.
  - typedef-var-1: a name declared again that may be legal.
  - vla-21: the file-scope VLA type name above.
  - pr59992: a translation limit, 8,191 names.

### What would find more

- **YARPGen**, which writes only valid C, so that fewer programs are thrown
  away than Csmith's three in five.
- **The gcc.dg must-refuse tests**, read one at a time, which is how the
  refusals of valid C99 were worked through.
