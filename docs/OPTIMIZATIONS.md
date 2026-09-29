# What acc does to make good code

acc compiles in one pass, with no syntax tree and no intermediate form
([DESIGN.md](DESIGN.md)), so it cannot run optimisation passes over a
function after the fact. What it does instead falls into three kinds:

- **Deciding late.** A value is a description on the value stack until
  something needs it in a register, so an operator sees its operands
  before any code exists for them and can choose the instruction that
  fits: fold two constants, use an immediate, read straight from a frame
  slot or a fixed address.
- **Looking back.** A sequence just emitted leaves a *mark* -- where it
  ended, and the count of rewinds, `out_rewinds`, when it was made. The
  next operation can check the mark, take the sequence back with
  [`out_rewind()`](../src/image.c#L547), and emit something better. `x < y` followed by a
  branch becomes one conditional jump this way.
- **Settling at the end.** A few things can only be decided once a
  function or a file is complete: which jumps reach with a short `jr`,
  which static functions nothing calls, which runtime routines are used.

What each is worth is measured against agondev in
[performance.md](performance.md), with `test/perf.sh` and `test/size.sh`.
Every one here is chosen for size first and speed second: on the Agon a
program's image is its heap.

The costs quoted are eZ80 ADL-mode bytes; the cycle figures are from the
emulator's cycle counter.

How to write C that makes the most of these is in
[writing-fast-c.md](writing-fast-c.md).

---

## 1. Constant folding

### Integers

[`vbinop()`](../src/arith.c#L449) folds any operator whose operands are both constants
with [`const_fold()`](../src/arith.c#L195), and pushes the result; no code is emitted.
Results wrap at 24 bits, as they would at run time, even on a host with a
32-bit int. Signedness follows C: a shift takes it from its left operand,
any other operator is unsigned if either side is. Division or remainder by
zero, and shifts by a negative count or by 24 or more, are left for run
time rather than given the host's answer.

Comparisons ([`vcmp()`](../src/arith.c#L1228)), `-x` and `~x` fold the same way. An
unsigned comparison of constants folds only when neither has its top bit
set, which is enough for `sizeof x == 3` and keeps a comparison with a
negative value honest.

`long`, `long long` and `float` constants fold in [`vbinop_long()`](../src/wide.c#L1036)
and [`vcmp_wide()`](../src/wide.c#L1248), the floats with acc's own soft-float routines so
that the host and the Agon give the same bits. Conversions between them
fold in [`vconvert()`](../src/vstack.c#L218).

### Addresses

The address of a file-scope variable is a constant: `VAL_ADDR` for one in
the image, `VAL_BSS` for an offset into the bss. [`fold_addr()`](../src/arith.c#L342)
lets address ± number stay an address of the same kind, and address −
address of the same kind become a number, so `&a[3]`, `&s.f` and `p - q`
over one array are constants. [`foldable()`](../src/arith.c#L383) refuses to fold a
comparison of an address whose value is not known yet against a number:
the first bss variable is at offset 0, and `&first == 0` must not fold to
true.

### Whole expressions

`&&`, `||` and `?:` with a constant condition fold in the parser
([`logical_rest()`](../src/expr.c#L1344), [`conditional_rest()`](../src/expr.c#L1978)): the side that is
never evaluated is parsed under a `GenMark` and its code rolled back with
[`gen_rollback()`](../src/vstack.c#L972). So `DEBUG && log(x)` costs nothing when `DEBUG`
is 0, and `1 && 1` is a constant expression.

### Operand order

A constant on the left of a commutative operator is moved to the right,
where it can be an immediate; and when the right operand is already in HL
and the left is not, the two are swapped, so `t[c]` does not move HL
through the stack. `x + 0` and `x - 0` emit nothing.

## 2. Arithmetic by a constant

| expression | code | against |
|---|---|---|
| `x + 3`, `p++` (step ≤ 4) | `inc hl` ×3 | `ld bc,3; add hl,bc`, 5 bytes |
| `x << 5` (count ≤ 8) | `add hl,hl` ×5 | a call to the shift helper |
| `x * 10` | shifts and adds, below | `call` to the multiply helper, 8 bytes with its setup |
| `x & 0x7f` | `ld a,l; and 7fh; sbc hl,hl; ld l,a` | a call to the AND helper |
| `x & 0x8000` | the low byte kept in IYL, top cleared | 6 bytes against 8 |

**Small steps.** A constant within ±4 (`STEP_MAX`) is added with `inc hl`
or `dec hl`, one byte and one cycle each.

**Left shifts** by a constant up to 8 are that many `add hl,hl`. There is
no matching right shift: nothing shifts HL's top byte right.

**Multiplies** by a constant from 1 to 65,535 are shifts and adds,
[`mul_const()`](../src/arith.c#L115): a power of two is only `add hl,hl`s; any other
constant is `push de; push hl; pop de`, then for each lower bit an `add
hl,hl` and, where the bit is set, an `add hl,de`, then `pop de`. It is
used up to 12 steps (`MUL_MAX_STEPS`). zap's commonest multiply, by 13 --
a struct's width -- is 9 bytes this way against the call's 8, and runs in
about a thirtieth of the time.

**AND, OR and XOR with a constant** ([`bitwise_const()`](../src/arith.c#L21)) work a
byte at a time through A, touching only the bytes the constant changes:

- A mask that fits the low byte is `ld a,l; and n; sbc hl,hl; ld l,a` --
  `and` clears carry, so `sbc hl,hl` zeroes the upper two bytes. If the
  byte was just loaded into A, the `ld a,l` is not needed.
- A mask with bits in the middle byte and none above (`& 0x8000`, `&
  0xffff`) keeps the low byte in IYL while `sbc hl,hl` clears the top.
- Otherwise, if the constant's top byte leaves that byte unchanged (`0xff`
  for AND, `0` for OR and XOR), the low and middle bytes that change go
  through A. Only a constant that changes the top byte calls the helper.

83% of the ANDs zap executes have a constant right operand.

**Known-narrow values.** A register value whose upper bytes are known to be
zero carries `VQ_BYTE` or `VQ_WORD`, read by `vwidth` in [`gen_int.h`](../src/gen_int.h): an
unsigned narrow load, a byte dereference, a byte return, and the result of
a narrow AND all set it. [`bitwise_narrow()`](../src/arith.c#L405) then does `&`, `|` and
`^` of two register values a byte or two at a time in A. An AND is as
narrow as its narrower side; OR and XOR need both.

## 3. Addresses in loads and stores

A global's address is a constant (section 1), so what is at it can be read
and written with the eZ80's absolute forms:

| C | code | bytes |
|---|---|---|
| `x = g;` (3-byte `g`) | `ld hl,(g)` | 4 |
| `c = gc;` (byte) | `ld a,(gc)` | 4 |
| `g = x;` | `ld (g),hl` (or `de`, `bc`) | 4 |
| `gc = c;` | `ld (gc),a` | 4 |
| `*p = 0;` | `ld (hl),0` | 2 |
| `*p = x;` | `ld (hl),de` | 2 |

**Reads.** [`vderef()`](../src/lvalue.c#L224) checks whether the pointer was just loaded
with `ld hl,nn`; if so it rewrites that instruction's opcode into `ld
hl,(nn)` or `ld a,(nn)` -- four bytes where the load and the dereference
were five or six. Not for 2-byte values, and not when something jumps in
between the two.

**Stores.** [`vstore_indirect()`](../src/lvalue.c#L402) stores to a constant address with
`ld (nn),rr` or `ld (nn),a`, taking A from an immediate, from A as it is,
from a byte local without widening it, or from L, E or C. Through a
pointer, a constant is `ld (hl),n` or `ld de,n; ld (hl),de`, and a 3-byte
register value is `ld (hl),de` or `ld (hl),bc`, with `ex de,hl` when the
value and the address are the other way round.

**Externs plus a constant.** An extern's address is `ld hl,0` and a fixup.
If a constant is added straight after (`g.b`, `arr[3]`), it goes into the
slot ([`out_add24()`](../src/image.c#L668)), and the link adds the symbol to it: one
load, where the add would be three instructions.

**Locals.** `&local` is `lea rr,ix+d`. A slot past `(ix-128)` is reached
through IY, `lea iy,ix+step` and if need be `lea iy,iy+step`
([`far_base()`](../src/insn.c#L116)).

## 4. Byte arithmetic without promotion

C promotes a `char` to `int` before any arithmetic. For `+ - & | ^ <<`,
truncating as you go gives the same low byte as promoting and truncating
at the end, so when the result goes straight into a byte, acc works in A
and never widens.

The parser sets `narrow_dest` to the width of what the value goes into --
a byte local, a compound assignment, a store through a byte pointer -- and
[`binary_rest()`](../src/expr.c#L1408) passes it to [`vapply()`](../src/arith.c#L1385) only when the
operator is one of those (`transparent_op`) and the expression ends at it
(`expression_ends_here`). In `c = a + b > 3` the `+` is not narrowed,
because its full value feeds the `>`.

[`vnarrow_ready()`](../src/arith.c#L917) accepts byte operands that are constants, byte
locals of the same type, or A itself; [`vbinop_narrow()`](../src/arith.c#L952) then emits

```
ld  a,(ix+b)
add a,(ix+c)        ; or sub, and, or, xor; or a,n for a constant
                    ; result stays in A as VAL_ACC
```

and [`vstore_local()`](../src/lvalue.c#L20) stores it with `ld (ix+d),a`. A constant
shift of a byte is that many `sla a`, `srl a` or `sra a`. Shorts are left
out: 16 bits is the eZ80's worst width in ADL mode, and the same six
operations measured 207 bytes as `unsigned char`, 277 as `unsigned int`
and 406 as `unsigned short`.

A value in A is widened only when something needs it as an int
([`force_reg()`](../src/vstack.c#L731)).

**Narrow loads.** Widening uses the `sbc hl,hl` idiom: `or a; sbc hl,hl`
is zero, and `ld l,a; rlc l; sbc hl,hl` is A's sign. An unsigned byte or
short loads straight into DE or BC (`ld de,0; ld e,(ix+d)`) when that is
where it is wanted, as the index in `t[c]` is.

## 5. Undoing a conversion nobody reads

Three marks let a widening or a conversion be taken back when what follows
does not need it:

- **The widen mark**, [`widen_loaded()`](../src/branch.c#L243), set after `ld a,(hl)` and
  after a call that returns a byte. [`widen_undo()`](../src/branch.c#L255) takes the
  widening back when the next operation wants the byte in A:
  `*p == '\n'` is `ld a,(hl); cp 10`, `while (*p)` is `ld a,(hl); or a`.
  [`store_byte_widened()`](../src/branch.c#L291) makes `c = *p;` into `ld a,(hl); ld
  (ix+d),a`. [`widen_again()`](../src/branch.c#L314) lets a cast to another byte type,
  `(unsigned char)*p`, redo only the widening that type needs.
- **The conversion mark**, set when a store to a narrow local converts the
  stored value back into the expression's result. [`gen_discard()`](../src/arith.c#L710)
  takes it back when the statement's value is thrown away, so `c = x;`
  does not pay to re-widen `c`, and `i++;` loses its step back (section 8).
- **Conversions that are nothing**: [`vconvert()`](../src/vstack.c#L218) emits no code for
  a conversion to the same type, from a narrow local to a type of the same
  width (the load is the conversion), for a register value already narrow
  enough, or for a wide local relabelled to a narrower wide type.

## 6. Comparisons and branches

A comparison makes 0 or 1 in HL, `ld hl,1; jp cc,L; ld hl,0; L:`, and
leaves a mark ([`cmp_value()`](../src/arith.c#L1304)). Almost nothing wants that value:
[`jump_on_truth()`](../src/branch.c#L380) takes it back and emits one conditional jump on
the flags ([`jump_on_flags()`](../src/branch.c#L193)). That is nineteen bytes a
comparison, which in a real program is a fifth of its image.
[`vtruth()`](../src/branch.c#L541) takes it back the same way for `!`, so `!(a < b)` is
`a >= b`, and `x != 0` after a comparison is the comparison.

**Unsigned where it can be.** An unsigned comparison is a subtract and one
jump on carry. A signed one needs the overflow flag too: three jumps and
fourteen bytes where the carry is one jump. So:

- [`cmp_is_unsigned()`](../src/arith.c#L298) uses the unsigned form when neither side can
  be negative;
- [`signed_as_unsigned()`](../src/arith.c#L1166) otherwise adds `0x800000` to both sides,
  which maps signed order onto unsigned order. Against a constant the
  constant is adjusted at compile time, and `x > c` becomes `x >= c+1` so
  that `x` stays in HL.

**Bytes against constants.** [`cmp_byte_const()`](../src/arith.c#L1099) compares a byte
local, or a byte just read, with a constant in its range in A: `ld
a,(ix+d); cp n`. `c > k` becomes `c >= k+1`; a signed byte has `xor 80h`
applied to both sides first.

**Tests against zero.** `x == 0` is `add hl,bc; or a; sbc hl,bc`
([`hl_zero_test()`](../src/arith.c#L1157)), 4 bytes against 6 for loading a zero. A value
known to be one or two bytes wide is tested with `ld a,l; or a` or `ld a,l;
or h`. An AND with a mask leaves its flags for the branch
([`flags_say_nonzero()`](../src/arith.c#L1024)), so `if (c & 0x8000)` is the `and` and a
`jp z`. A `long` or `long long` against zero ORs its bytes into A.

**`&&` and `||`.** [`gen_logic_left()`](../src/branch.c#L575) produces a 0/1 value with a
list of jump holes behind it ([`logic_keep()`](../src/branch.c#L356)); a branch on it
takes the value back and relinks the holes to its own targets
([`logic_chain()`](../src/branch.c#L369)). So `while (p < e && ok(*p))` jumps out from
each side with no 0/1 made.

## 7. Registers and the frame

**Lazy loading.** A constant stays a number and a local a frame offset
until an operator needs it in a register. Spilling
([`reg_alloc()`](../src/vstack.c#L687)) takes the oldest register value, and moves only
the entry that is in the way, to a free register if there is one.

**Reload after store.** [`ld_rr_ix()`](../src/gen_int.h#L323) skips `ld rr,(ix+d)` when
the instruction before it was `ld (ix+d),rr` of the same register and
slot, and no jump lands between them. The store that ends one statement
and the load of the same variable that starts the next are a twentieth of
zap's running time.

**Scratch.** Spill slots are reused within a statement and the area resets
at its end ([`gen_stmt_end()`](../src/vstack.c#L632)), so a frame holds the most any one
statement needs, not the sum.

**A local in IY.** One local or parameter a function declares `register`,
an int or a pointer, lives in IY instead of its frame slot
([`gen_iy_claim()`](../src/vstack.c#L253)). It is `VAL_IY` on the value stack, with a
displacement: adding a small constant changes only the displacement, so
`p + 1` is no code and `p++;` is `inc iy`, and memory through it is
`(iy+d)`. A read into another register is `lea rr,iy+d`. IY is the
backend's scratch elsewhere, so in such a function every call and every
use of IY for something else pushes it first and pops it after.

**Locals within reach.** Locals are kept in the first 96 bytes
(`NEAR_LOCALS`) of the frame, inside `(ix+d)`'s reach, with room left for
scratch. Over 1,012 functions, the most locals any had was 84 bytes.

## 8. Increment and decrement

`x++` on an int or pointer local, whose value is used, is "change, store,
step back" -- `inc hl; ld (ix+d),hl; dec hl` -- rather than keeping a copy in a second
register ([`vpostfix_local()`](../src/lvalue.c#L849)). When the value is not used, as in
`x++;`, [`gen_discard()`](../src/arith.c#L710) removes the step back. `c++` on a byte runs
in A.

## 9. `long`, `long long` and `float`

These live in frame slots and their arithmetic calls the runtime with
`lea hl,ix+L; lea de,ix+R; call helper`, the result written over the left
operand.

- **Operands in place.** An operand already in a frame slot of its width is
  read where it is, unless the helper writes its right operand (division,
  float subtract and compare): `a + b` copies `a` and reads `b`.
- **Scratch reused.** Each result is built in the lowest dead operand's
  slot ([`spill_lowest()`](../src/wide.c#L451)).
- **A constant pool.** A wide constant operand is `ld de,<pool entry>`, 4
  bytes, rather than written into a slot, 16 bytes for a `long` and again
  at every use. The pool is laid down after the function
  ([`pool_emit()`](../src/wide.c#L734)), with only the entries still referenced after any
  rollback.
- **Constants on bytes.** [`long_const_bytes()`](../src/wide.c#L803) does a 4-byte `&`,
  `|` or `^` with a constant, and shifts by 1, 8, 16 or 24, on the slot's
  bytes directly: identity bytes are skipped, `| 0xff` is `ld
  (ix+d),0ffh`, `^ 0xff` is `cpl`, zero bytes come from one `xor a`. It is
  used when it comes in under the 26 bytes (`LONG_CALL_BYTES`) of copying
  and calling.
- **Into the variable.** `x = x * k + 1` on a `long` would build its result
  in scratch and copy it out; [`long_into()`](../src/wide.c#L983) redoes up to four
  chained calls into the destination instead, with no copies.
- **Three bytes at a time through IY.** A `long` copies as `ld
  iy,(ix+s); ld (ix+d),iy` plus a byte, 12 bytes against 24
  ([`copy_long()`](../src/wide.c#L41)); a constant is written three bytes at a time
  ([`wide_bytes_at()`](../src/wide.c#L353)), and the load into IY is skipped when the
  next three bytes are the same.
- **Float negation** is `xor 80h` on the top byte. `f - c` for a constant
  `c` is `f + (-c)`, so the pool can hold it.
- **The runtime's division** runs its shift-and-subtract loop entirely in
  registers, starting from the dividend's first nonzero byte: 24 turns for
  a value under 2²⁴, 16 under 2¹⁶. The 64-bit multiply skips zero rows, and
  the 64-bit divide has a fast path for a one-byte divisor.

## 10. Jumps

**Short jumps.** Every jump is emitted as a 3-byte `jp` through
[`jump_op()`](../src/branch.c#L76) and recorded. At the end of each function,
[`relax_function()`](../src/relax.c#L609) shortens every one whose condition has a `jr`
form (always, Z, NZ, C, NC) and whose target is in reach, cutting the two
bytes. Reach is judged before anything shrinks, and shrinking only brings
targets closer, so one pass is enough. This saves about 4% of an image.
Doing it once per function rather than once per file keeps its cost small
and its tables short.

**A jump over a jump.** `jp cc,L1; jp L2; L1:` becomes `jp !cc,L2`
([`branch_over()`](../src/relax.c#L586)) when nothing else jumps to the second jump. That
is every `if (x) break;`, `if (x) continue;`, and a `return` that jumps to
an earlier return of the same constant.

**Loops.** A `for` loop is test, body, step, and one jump back to the
test: its step is recorded as text and compiled after the body
([`step_again()`](../src/stmt.c#L729)). `do … while` is one conditional jump back. A
backward jump is recorded so that it can be shortened too
([`gen_jump_to()`](../src/branch.c#L161)).

**`switch`** is a chain of compares after the body. The value is loaded
once ([`gen_switch_load()`](../src/branch.c#L483)); each case is `ld de,v; or a; sbc
hl,de; add hl,de; jp z,case` ([`gen_switch_case()`](../src/branch.c#L499)) -- the `add`
puts HL back and leaves Z alone, so the value is never reloaded. A `long`
case compares its top byte in A first and skips the rest on a mismatch.
There are no jump tables.

## 11. Functions

**The prologue is a call.** `ld hl,-frame; call acc_rt_frameset`, 8 bytes
where the sequence it runs is 13 ([`gen_func_begin()`](../src/func.c#L388)). A function
with no locals calls `acc_rt_frameset0` and has no load.

**One epilogue.** Every `return` jumps to one `ld sp,ix; pop ix; ret` at
the end of the function. A jump is two bytes once shortened and the
epilogue five, and zap's functions average 4.5 returns each. A `return`
that would jump to the next byte is taken back.

**Constant returns shared.** A second `return 0;` jumps to the first one's
`ld hl,0` rather than repeating it ([`gen_return()`](../src/func.c#L556)): 2 bytes against
9, for up to eight constants a function.

**Local arrays** take their address as `push de; ld de,K; push ix; pop hl;
add hl,de; pop de` while the frame's size is unknown. At the end,
[`gen_func_end()`](../src/func.c#L430) rewrites every one within reach as `lea hl,ix+d`
and cuts the other seven bytes.

**Built-in calls.** `memcpy`, `memmove`, `memset` and `memchr` are calls to
runtime routines built on `ldir` and `cpir`, with their operands in
registers ([`mem_builtin()`](../src/func.c#L769)); a byte loop in C cost zap 7% of its
time. `exit` is four instructions in line ([`exit_builtin()`](../src/func.c#L729)).
Struct copies are `ldir`.

**Byte results** are returned in A as well as HL, and the caller treats A
as just loaded (section 5), so `if (f())` tests A.

## 12. Inline functions

A call costs about 60 cycles before the callee does anything, and zap
calls its character-class tests hundreds of thousands of times. A `static
inline` function whose body is one `return` of an int-or-narrower value,
with at most eight int-or-narrower parameters and no varargs, is expanded
at each call: its text is recorded as it is compiled
([`return_kept()`](../src/inline.c#L211)), and [`inline_expand()`](../src/inline.c#L251) stores the
arguments in scratch slots and parses the text again with the parameter
names bound to them. Every other name in the text must still mean what it
meant at the definition, or the call is made normally. Nesting stops at
four, which also stops recursion.

## 13. Only what is used

**Unused static functions.** A `static` or `static inline` function that
nothing outside the unused ones calls or takes the address of is cut out
at the end of the file ([`drop_unused_statics()`](../src/relax.c#L782)), working outward
from what is used so that mutually recursive dead functions go too. A
header of `static inline` helpers costs a file only the ones it calls.

**Runtime routines.** The runtime is a library, `rt.a`, one object for each
routine or group of routines that share code ([`lib/rt/`](../lib/rt)), and
a link takes the objects a program calls and no others. One multiply does
not bring the float routines with it.

**Zero-initialised globals** go in the bss, after the image: they take no
room on the card, and are cleared by the startup with `ld (hl),0` and an
`ldir` ([`bss_emit()`](../src/finish.c#L419)). A one-byte bss is cleared without the
`ldir`, which with a count of zero would clear 16 MB.

**Library members.** A link takes from a library only the items -- single
functions and objects -- that are wanted, and what they reach
([`take_items()`](../src/link.c#L170)).

## 14. Smaller things

- A short narrows through IY, so DE is not disturbed.
- A local array is zeroed by clearing one byte and copying it along with
  `ldir`.
- A call through a pointer loads IY straight from a constant or a frame
  slot.
- A string literal inside a function is laid down where it is used and
  jumped over, so its address is a constant from the moment it is written.
