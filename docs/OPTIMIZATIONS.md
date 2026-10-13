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
  [`out_rewind()`](../src/image.c#L558), and emit something better. `x < y` followed by a
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

[`vbinop()`](../src/arith.c#L475) folds any operator whose operands are both constants
with [`const_fold()`](../src/arith.c#L214), and pushes the result; no code is emitted.
Results wrap at 24 bits, as they would at run time, even on a host with a
32-bit int. Signedness follows C: a shift takes it from its left operand,
any other operator is unsigned if either side is. Division or remainder by
zero, and shifts by a negative count or by 24 or more, are left for run
time rather than given the host's answer.

Comparisons ([`vcmp()`](../src/arith.c#L1368)), `-x` and `~x` fold the same way. An
unsigned comparison of constants folds only when neither has its top bit
set, which is enough for `sizeof x == 3` and keeps a comparison with a
negative value honest.

`long`, `long long` and `float` constants fold in [`vbinop_long()`](../src/wide.c#L1166)
and [`vcmp_wide()`](../src/wide.c#L1368), the floats with acc's own soft-float routines so
that the host and the Agon give the same bits. Conversions between them
fold in [`vconvert()`](../src/vstack.c#L303).

### Addresses

The address of a file-scope variable is a constant: `VAL_ADDR` for one in
the image, `VAL_BSS` for an offset into the bss. [`fold_addr()`](../src/arith.c#L366)
lets address ± number stay an address of the same kind, and address −
address of the same kind become a number, so `&a[3]`, `&s.f` and `p - q`
over one array are constants. [`foldable()`](../src/arith.c#L407) refuses to fold a
comparison of an address whose value is not known yet against a number:
the first bss variable is at offset 0, and `&first == 0` must not fold to
true.

### Whole expressions

`&&`, `||` and `?:` with a constant condition fold in the parser
([`logical_rest()`](../src/expr.c#L1402), [`conditional_rest()`](../src/expr.c#L2037)): the side that is
never evaluated is parsed under a `GenMark` and its code rolled back with
[`gen_rollback()`](../src/vstack.c#L1093). So `DEBUG && log(x)` costs nothing when `DEBUG`
is 0, and `1 && 1` is a constant expression.

### Operand order

A constant on the left of a commutative operator is moved to the right,
where it can be an immediate, or for a `long` or `float` come from the
pool (section 9); and when the right operand is already in HL
and the left is not, the two are swapped, so `t[c]` does not move HL
through the stack. `x + 0` and `x - 0` emit nothing.

## 2. Arithmetic by a constant

| expression | code | against |
|---|---|---|
| `x + 3`, `p++` (step ≤ 4) | `inc hl` ×3 | `ld bc,3; add hl,bc`, 5 bytes |
| `x << 5` (count ≤ 8) | `add hl,hl` ×5 | a call to the shift helper |
| `x >> 2` (count ≤ 8), `x / 4` | A filled from the top, then a call into `shr` or `sdiv` | the shift loop, the divide |
| `x * 10` | shifts and adds, below | `call` to the multiply helper, 8 bytes with its setup |
| `x & 0x7f` | `ld a,l; and 7fh; sbc hl,hl; ld l,a` | a call to the AND helper |
| `x & 0x8000` | the low byte kept in IYL, top cleared | 6 bytes against 8 |

**Small steps.** A constant within ±4 (`STEP_MAX`) is added with `inc hl`
or `dec hl`, one byte and one cycle each.

**Left shifts** by a constant up to 8 are that many `add hl,hl`. Nothing
shifts HL's top byte right, so a right shift by 1 to 8 goes the other way
([`shr_const()`](../src/insn.c#L574)): with A holding what fills from the top (`xor a`,
or `push hl; add hl,hl; pop hl; sbc a,a` for the sign), the runtime's
`acc_rt_shr`, entered 2(k - 1) bytes in, shifts A:HL left by 8 - k and
keeps its top three bytes, which it reads back through the stack
([`lib/rt/shrk.s`](../lib/rt/shrk.s)). The call's slot holds the offset and the
link adds the routine's address to it. No count is loaded and nothing
loops: `x >> 8` is 38 cycles where the loop was 253. A shift by 16 to 23
is HL's top byte, shifted in A.

**Divides and remainders by a power of two.** An unsigned divide by 2 to
256, or by 2^16 to 2^23, is the right shift above. A signed divide by 2
to 256 calls `acc_rt_sdiv` the same way ([`lib/rt/sdivk.s`](../lib/rt/sdivk.s)):
the same shift, plus one for a negative value that had bits shifted out,
since division rounds toward zero. `x / 4` is 59 cycles where the divide
was 605. An unsigned remainder by a power of two is an AND with one less.

**Pointer differences.** Two pointers into one array are a whole number
of elements apart, so their difference divides by the element's width
exactly. [`exact_divide()`](../src/lvalue.c#L184) makes that a signed shift
for the twos in the width and a multiply by the inverse of the odd rest
modulo 2^24, which undoes multiplying by it: `p - q` on `int *` or on a
pointer to pointers is a multiply by 0xAAAAAB, the inverse of 3, rather
than a signed divide by 3. [`mul_const()`](../src/arith.c#L126) sends a
multiply by 0xAAAAAB, wherever a program makes one, to a routine of its
own, [`acc_rt_mulinv3`](../lib/rt/mulinv3.s): 0xAAAAAB is -0x555555, and
0x555555 is 5 * 17 * 257 * 65537 as far as 24 bits go, so it is four
shift-and-adds -- the shifts by 8 and 16 whole bytes moved through the
stack -- and a negation, about half the cycles of the general multiply.
No constant is loaded into BC for it. A VLA row's width is known only at
run time, and that difference still divides.

**Multiplies** by a constant from 1 to 65,535 are shifts and adds,
[`mul_const()`](../src/arith.c#L126): a power of two is only `add hl,hl`s; any other
constant is `push de; push hl; pop de`, then for each lower bit an `add
hl,hl` and, where the bit is set, an `add hl,de`, then `pop de`. It is
used up to 12 steps (`MUL_MAX_STEPS`). zap's commonest multiply, by 13 --
a struct's width -- is 9 bytes this way against the call's 8, and runs in
about a thirtieth of the time.

**Without a constant.** A multiply, divide or remainder of two values
calls the runtime, HL by BC into HL. `acc_rt_mul`
([`lib/rt/mul.s`](../lib/rt/mul.s)) builds the product from MLT's 8x8
products of the bytes, leaving out the pairs that land above bit 23, and
works in A and DE alone: the operands' top bytes, which have no register
names, are read by pushing both a byte apart and popping D from between
them. It is 100 cycles a call with the call itself, where reading the
operands from a frame through IY was 163. The divide
([`lib/rt/udivmod.s`](../lib/rt/udivmod.s)) shifts the dividend into a
remainder a bit at a time, 24 rounds -- 16, or 8, where the dividend's top
byte, or two, is zero, since a zero shifted in adds nothing to the
remainder or the quotient. A thousand remainders of 50000 by 7 take 433
thousand cycles so, against 556 thousand at 24 rounds each. The divisor's
zero test is made in place, not called.

**AND, OR and XOR with a constant** ([`bitwise_const()`](../src/arith.c#L26)) work a
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
unsigned narrow load, a byte dereference, a byte return, and an AND whose
constant keeps nothing above the low byte, or above the low two, all set
it: [`bitwise_const()`](../src/arith.c#L26) answers how many bytes of its
result can be other than zero. [`bitwise_narrow()`](../src/arith.c#L429) then does `&`, `|` and
`^` of two register values a byte or two at a time in A. An AND is as
narrow as its narrower side; OR and XOR need both. Such a value made a
`long` needs no sign test (section 9).

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
| `x = p->m;` (`p` a local) | `ld iy,(ix+d); ld hl,(iy+n)` | 6 |

**Reads.** [`vderef()`](../src/lvalue.c#L428) checks whether the pointer was just loaded
with `ld hl,nn`; if so it rewrites that instruction's opcode into `ld
hl,(nn)` or `ld a,(nn)` -- four bytes where the load and the dereference
were five or six. Not for 2-byte values, and not when something jumps in
between the two.

**Stores.** [`vstore_indirect()`](../src/lvalue.c#L729) stores to a constant address with
`ld (nn),rr` or `ld (nn),a`, taking A from an immediate, from A as it is,
from a byte local without widening it, or from L, E or C. Through a
pointer, a constant is `ld (hl),n` or `ld de,n; ld (hl),de`, and a 3-byte
register value is `ld (hl),de` or `ld (hl),bc`, with `ex de,hl` when the
value and the address are the other way round.

**Globals defined elsewhere.** A global that is extern, or defined further
down the file, has an address only the link knows: it is loaded at once,
`ld hl,0` and a fixup, into whichever pair is free. If a constant is added
straight after (`g.b`, `arr[3]`), it goes into the slot
([`out_add24()`](../src/image.c#L681)), and the link adds the symbol to it: one
load, where the add would be three instructions. A read where that load
was the last thing made is made from the address, as a global of the file's
is: [`vderef()`](../src/lvalue.c#L428) turns the load into `ld hl,(nn)` or `ld
a,(nn)`, or, where HL was holding something and the address went to DE or
BC, into `ld de,(nn)` or `ld bc,(nn)`, a byte longer, with the value left
in that pair -- where it was `ld de,nn; push hl; pop bc; ex de,hl; ld
hl,(hl)`. So `(a << 1) + g` is `add hl,hl; ld de,(g); add hl,de`. An int
stored to one, from a number or an int local, which make no code until
they are wanted, takes the load back the same way and writes to the
address itself ([`store_to_extern()`](../src/lvalue.c#L710)): `ld hl,5; ld
(g),hl`, eight bytes, or seven from a local, where `ld hl,g; ld de,5; ld
(hl),de` was ten or nine. The fixup moves with the address, and what was
added to it is kept.

**Members through a pointer.** `p->m`, with `p` a local, loads `p` into
HL, adds the member's offset -- an `inc hl` a byte, or `ld de,n; add hl,de`
past four -- and reads through HL: seven to ten bytes. Where the offset is
under 128 and IY holds no `register` local (section 7),
[`member_through_iy()`](../src/lvalue.c#L414) takes the load and the add
back and reads from IY instead:

```
ld  iy,(ix+d)       ; p
ld  hl,(iy+n)       ; or ld a,(iy+n) for a byte
```

six bytes. `p->m = v` does the same where `v` makes no code before the
store -- a number, or a local as wide as the member
([`member_in_iy()`](../src/lvalue.c#L387)): `ld (iy+n),hl` for an int, `ld
(iy+n),a` for a byte local, `ld (iy+n),k` for a byte constant. A short
member is left as it was, and so is any case where the rewrite would not
be smaller.

IY then stays loaded with `p`. The next member of the same pointer, `p->b`
in the statement after `p->a`, is reached without loading IY again when
[`iy_kept()`](../src/lvalue.c#L342) finds that the code in between leaves
IY and `p`'s slot alone -- loads and stores through IX and IY, none of them
to that slot, the pairs' moves and sums, jumps away -- with nothing jumping
in between and no more than 128 bytes of it. Not across a call, since a
callee is free with IY, and not in a function that takes a local's address,
where `p` could change with no store to its slot.

**Locals.** `&local` is `lea rr,ix+d`. A slot past `(ix-128)` is reached
through IY, `lea iy,ix+step` and if need be `lea iy,iy+step`
([`far_base()`](../src/insn.c#L255)).

## 4. Byte arithmetic without promotion

C promotes a `char` to `int` before any arithmetic. For `+ - & | ^ <<`,
truncating as you go gives the same low byte as promoting and truncating
at the end, so when the result goes straight into a byte, acc works in A
and never widens.

The parser sets `narrow_dest` to the width of what the value goes into --
a byte local, a compound assignment, a store through a byte pointer -- and
[`binary_rest()`](../src/expr.c#L1466) passes it to [`vapply()`](../src/arith.c#L1525) only when the
operator is one of those (`transparent_op`) and the expression ends at it
(`expression_ends_here`). In `c = a + b > 3` the `+` is not narrowed,
because its full value feeds the `>`.

[`vnarrow_ready()`](../src/arith.c#L977) accepts byte operands that are constants, byte
locals of the same type, or A itself; [`vbinop_narrow()`](../src/arith.c#L1012) then emits

```
ld  a,(ix+b)
add a,(ix+c)        ; or sub, and, or, xor; or a,n for a constant
                    ; result stays in A as VAL_ACC
```

and [`vstore_local()`](../src/lvalue.c#L64) stores it with `ld (ix+d),a`. A constant
shift of a byte is that many `sla a`, `srl a` or `sra a`. Shorts are left
out: 16 bits is the eZ80's worst width in ADL mode, and the same six
operations measured 207 bytes as `unsigned char`, 277 as `unsigned int`
and 406 as `unsigned short`.

A value in A is widened only when something needs it as an int
([`force_reg()`](../src/vstack.c#L827)). A byte left in A -- the value of
`--w` or `c = x + 1` -- is moved to a register before the right operand of
an operator is read, and after each argument of a call
([`vacc_out()`](../src/vstack.c#L478)), because reading a global byte or a
byte through a pointer goes through A. It is widened as a byte just read
is, so where the right operand needs no code, a number or a local, the
widening is taken back and the byte compared in A as before (section 5).

**Narrow loads.** Widening uses the `sbc hl,hl` idiom: `or a; sbc hl,hl`
is zero, and `rla; sbc hl,hl; rra` is A's sign
([`fill_hl_with_sign_of_a()`](../src/insn.c#L375)) -- `rla` puts the sign in
the carry, `sbc hl,hl` fills HL with it and leaves the carry as it was, and
`rra` puts back the bit `rla` took out, so A still holds the byte for the
`ld l,a` that follows. An unsigned byte or
short loads straight into DE or BC (`ld de,0; ld e,(ix+d)`) when that is
where it is wanted, as the index in `t[c]` is.

## 5. Undoing a conversion nobody reads

Three marks let a widening or a conversion be taken back when what follows
does not need it:

- **The widen mark**, [`widen_loaded()`](../src/branch.c#L267), set after `ld a,(hl)`,
  after a call that returns a byte, after a byte local is read into HL
  ([`widen_local()`](../src/branch.c#L278)), and wherever a byte in A is widened: for
  the value stack, by a conversion to a byte, and as the answer of `&`
  with a byte mask ([`widen_made()`](../src/branch.c#L295)). [`widen_undo()`](../src/branch.c#L317)
  takes the widening back when the next operation wants the byte in A --
  a byte local's is read into A again: `*p == '\n'` is `ld a,(hl); cp
  10`, `while (*p)` and `if (c)` are `or a` on the byte, `x & 4` of a byte
  local is `ld a,(ix+d); and 4`. `|` and `^` with a constant byte work on
  the byte and widen after, where that widening is the answer's too
  ([`widen_byte_op()`](../src/branch.c#L355)). A byte written through a pointer goes from
  A through the pair the pointer is in, `ld (hl),a`, when it is a byte
  already ([`store_byte_through()`](../src/lvalue.c#L682)).
  [`store_byte_widened()`](../src/branch.c#L382) makes `c = *p;` into `ld a,(hl); ld
  (ix+d),a`, and a byte just read and stored to a global of the file goes
  from A the same way, `ld a,(hl); ld (nn),a`. An `ld a,l` straight after
  a byte was widened from A into HL is left out
  ([`a_is_l()`](../src/branch.c#L336)): the widening leaves A as it was.
  [`widen_again()`](../src/branch.c#L403) lets a cast to another byte type,
  `(unsigned char)*p`, redo only the widening that type needs.
- **The conversion mark**, set when a store to a narrow local converts the
  stored value back into the expression's result. [`gen_discard()`](../src/arith.c#L762)
  takes it back when the statement's value is thrown away, so `c = x;`
  does not pay to re-widen `c`, and `i++;` loses its step back (section 8).
- **Conversions that are nothing**: [`vconvert()`](../src/vstack.c#L303) emits no code for
  a conversion to the same type, from a narrow local to a type of the same
  width (the load is the conversion), for a register value already narrow
  enough, or for a wide local relabelled to a narrower wide type.

## 6. Comparisons and branches

A comparison makes 0 or 1 in HL, `ld hl,1; jp cc,L; ld hl,0; L:`, and
leaves a mark ([`cmp_value()`](../src/arith.c#L1444)). Almost nothing wants that value:
[`jump_on_truth()`](../src/branch.c#L470) takes it back and emits one conditional jump on
the flags ([`jump_on_flags()`](../src/branch.c#L202)). That is nineteen bytes a
comparison, which in a real program is a fifth of its image.
[`vtruth()`](../src/branch.c#L741) takes it back the same way for `!`, so `!(a < b)` is
`a >= b`, and `x != 0` after a comparison is the comparison.

**Unsigned where it can be.** An unsigned comparison is a subtract and one
jump on carry. A signed one needs the overflow flag too: three jumps and
fourteen bytes where the carry is one jump. So:

- [`cmp_is_unsigned()`](../src/arith.c#L322) uses the unsigned form when neither side can
  be negative;
- [`signed_as_unsigned()`](../src/arith.c#L1292) otherwise: against 0 the sign is the
  answer, and `add hl,hl` puts it in the carry; against another constant
  it adds `0x800000` to both sides, which maps signed order onto unsigned
  order, the constant moved at compile time and `x > c` made `x >= c+1` so
  that `x` stays in HL. The moved constant is then taken away by adding
  2^24 less it, whose carry says `x >= c` where a subtract's would say `x
  < c`; the jump is turned over to match, and the subtract's `or a` is not
  needed. `x > 5` is

  ```
  ld  bc,800000h
  add hl,bc
  ld  bc,7ffffah      ; 2^24 - 800006h
  add hl,bc
  jr  nc,else
  ```

  Against the lowest int, whose moved constant is 0, it subtracts: an add
  of nothing never carries. Of two values, it subtracts, moves the
  difference's sign into the carry with `add hl,hl`, and turns the carry
  over where the subtract overflowed (`jp po; ccf`).

**Bytes against constants.** [`cmp_byte_const()`](../src/arith.c#L1159) compares a byte
local, or a byte just read, with a constant in its range in A: `ld
a,(ix+d); cp n`. `c > k` becomes `c >= k+1`; a signed byte has `xor 80h`
applied to both sides first. A signed byte ordered against an unsigned
constant, `c < 10u`, is not: C converts a negative `c` to a large unsigned
value, so that order is made at 24 bits. Equality, which signedness does
not change, is still the byte's.

**Two bytes.** [`cmp_bytes()`](../src/arith.c#L1235) compares two values
whose top two bytes are known to be zero -- unsigned chars, masks of them
(`VQ_BYTE`), or an unsigned char not yet loaded -- in A: `ld a,e; cp l`, or
`ld a,(ix+d); cp (ix+d)`. Both are 0 to 255, where a signed order and an
unsigned one are the byte's. A byte just read and widened is taken back
into A, from either side for `==` and `!=`. So `(x & 15) == mode`, with
`x` in HL and `mode` an unsigned char local, is `ld a,l; and 0fh; cp
(ix+d)`, not two 24-bit values subtracted with one moved through the stack
to make room. The answer is made in HL, so anything else there is moved
out first.

**Tests against zero.** `x == 0` is `add hl,bc; or a; sbc hl,bc`
([`hl_zero_test()`](../src/arith.c#L1283)), 4 bytes against 6 for loading a zero. A value
known to be one or two bytes wide is tested with `ld a,l; or a` or `ld a,l;
or h`; a mask of the low two bytes, `x & 0xfff`, is such a value. An AND with a mask leaves its flags for the branch
([`flags_say_nonzero()`](../src/arith.c#L1084)), so `if (c & 0x8000)` is the `and` and a
`jp z`. A `long` or `long long` against zero ORs its bytes into A.

**`&&` and `||`.** [`gen_logic_left()`](../src/branch.c#L774) produces a 0/1 value with a
list of jump holes behind it ([`logic_keep()`](../src/branch.c#L446)); a branch on it
takes the value back and relinks the holes to its own targets
([`logic_chain()`](../src/branch.c#L459)). So `while (p < e && ok(*p))` jumps out from
each side with no 0/1 made.

**`?:` on a comparison.** The values under a `?:`'s condition go to the
frame before the arms, so that both find them in the same place
([`gen_cond_begin()`](../src/branch.c#L872)). When the condition is a
comparison, its 0/1 is taken back first, the values are spilled, and the
jump is made on the flags the comparison left: a spill is `ld (ix+d),rr`,
or `lea`, `push` and `pop` for a far slot, and none of them touches the
flags. `x * 3 + (c < 26 ? 1 : 2)` jumps on the compare, where spilling
after it would leave only the 0/1 to test again.

## 7. Registers and the frame

**Lazy loading.** A constant stays a number and a local a frame offset
until an operator needs it in a register. Spilling
([`reg_alloc()`](../src/vstack.c#L783)) takes the oldest register value, and moves only
the entry that is in the way, to a free register if there is one.

**No reload.** [`ld_rr_ix()`](../src/insn.c#L172) skips `ld rr,(ix+d)` when the
last load or store of that register was of that slot, and what came after
it changes no pair and no memory -- reads through HL, A's operators and
tests, and jumps on, within 32 bytes -- with no jump landing between and
nothing taken back to before it. The store that ends one statement and
the load of the same variable that starts the next are a twentieth of
zap's running time; `*p == ' ' || *p == '\t'` reads `p` once. A byte is
treated the same way: a load into A of the slot A was just stored to is
left out ([`ld_a_ix()`](../src/insn.c#L331)), so `d = *p; f(d)` does not
read `d` back.

**Loop tops.** A load at a place something jumps to is never left out,
since a jump may arrive with the register holding something else. The top
of a loop is such a place, and `for (i = 0; i < n; i++)` stores `i` on the
way in and reads it again at once for the test. Where a store ends right
at a loop's top, [`gen_loop_top()`](../src/insn.c#L136) lets the test's
load of that slot into that pair be left out, and every jump back to the
top -- the loop's own and a `continue`'s -- goes through
[`gen_loop_back()`](../src/insn.c#L160), which loads the pair first unless
it holds the slot already, as it does after `i++`:

```
    ld   hl,0
    ld   (ix-3),hl      ; i = 0
top:
    ld   de,(ix+6)      ; n; i is in HL
    ...
    ld   hl,(ix-3)
    inc  hl
    ld   (ix-3),hl      ; i++, and HL holds i
    jr   top
```

A top that a forward jump also lands at is not used this way.

**Scratch.** Spill slots are reused within a statement and the area resets
at its end ([`gen_stmt_end()`](../src/vstack.c#L728)), so a frame holds the most any one
statement needs, not the sum.

**A local in IY.** One local or parameter a function declares `register`,
an int or a pointer, lives in IY instead of its frame slot
([`gen_iy_claim()`](../src/vstack.c#L266)). It is `VAL_IY` on the value stack, with a
displacement: adding a small constant changes only the displacement, so
`p + 1` is no code and `p++;` is `inc iy`, and memory through it is
`(iy+d)`. A read into another register is `lea rr,iy+d`. IY is the
backend's scratch elsewhere, so in such a function every call and every
use of IY for something else pushes it first and pops it after, and
members through a pointer are reached through HL, not IY (section 3).

**Locals within reach.** Locals are kept in the first 96 bytes
(`NEAR_LOCALS`) of the frame, inside `(ix+d)`'s reach, with room left for
scratch. Over 1,012 functions, the most locals any had was 84 bytes.

## 8. Increment and decrement

`x++` on an int or pointer local, whose value is used, is "change, store,
step back" -- `inc hl; ld (ix+d),hl; dec hl` -- rather than keeping a copy in a second
register ([`vpostfix_local()`](../src/lvalue.c#L1270)). When the value is not used, as in
`x++;`, [`gen_discard()`](../src/arith.c#L762) removes the step back. `x++` through a
pointer, or of a global, is stepped where it is and stepped back the same
way, narrowed to what was stored ([`vpostfix_indirect()`](../src/lvalue.c#L1314)): `n++;` of a global
is `ld hl,(n); inc hl; ld (n),hl`. Not a `_Bool`'s, a bit-field's, a
long's or a float's. `c++` on a byte runs in A.

## 9. `long`, `long long` and `float`

These live in frame slots and their arithmetic calls the runtime with
`lea hl,ix+L; lea de,ix+R; call helper`, the result written over the left
operand.

- **Operands in place.** An operand already in a frame slot of its width is
  read where it is. No routine writes its right operand: the divisions
  leave the remainder in a buffer of their own (`acc_rt_ldiv_buf`,
  `acc_rt_lldiv_buf`), where the signed ones also make a copy of the
  divisor positive, and the float subtract turns the right operand's sign
  over, adds, and turns it back. So a constant divisor comes from the pool
  as any other constant right operand does: `a % 65521UL` is `ld de,<pool
  entry>`.
- **The left operand copied by the routine.** The answer is written over
  the left operand, so a left operand that lies anywhere else has to be
  copied to the answer's slot first. Each binary routine has a second
  entry, its name with a 3 after it, that takes the left operand's address
  in BC as well and copies it to (HL) before falling into the routine
  ([`wide_apply()`](../src/wide.c#L1091)): three bytes of `lea bc` at the
  call, where the copy through IY was twelve, eighteen for a `long long`.
  The copy is shared, `acc_rt_wcopy4` a byte at a time through A and
  `acc_rt_wcopy8` an `ldir`. `r = a + b` is

  ```
  lea  hl,ix-4        ; r
  lea  de,ix+12       ; b
  lea  bc,ix+6        ; a, copied to (hl) first
  call acc_rt_ladd3
  ```

  An `ldir` copies upward, so a left operand only a few bytes below its
  destination is copied before the call instead.
- **Scratch reused.** Each result is built in the lowest dead operand's
  slot ([`spill_lowest()`](../src/wide.c#L551)).
- **A constant pool.** A wide constant operand is `ld de,<pool entry>`, 4
  bytes, rather than written into a slot, 16 bytes for a `long` and again
  at every use. The pool is laid down after the function
  ([`pool_emit()`](../src/wide.c#L830)), with only the entries still referenced after any
  rollback. A constant on the left of `&`, `|`, `^`, `+` or `*` is moved
  to the right first (section 1) so that it can come from the pool; for a
  `float` that is `+` and `*`, which IEEE answers the same either way round,
  so `2.5f * x` reads its constant from the pool.
- **Constants on bytes.** [`long_const_bytes()`](../src/wide.c#L903) does a 4-byte `&`,
  `|` or `^` with a constant, and shifts by 1, 8, 16 or 24, on the slot's
  bytes directly: identity bytes are skipped, `| 0xff` is `ld
  (ix+d),0ffh`, `^ 0xff` is `cpl`, zero bytes come from one `xor a`. It is
  used when it comes in under the 26 bytes (`LONG_CALL_BYTES`) of copying
  and calling.
- **Into the variable.** `x = x * k + 1` on a `long` would build its result
  in scratch and copy it out; [`long_into()`](../src/wide.c#L1121) redoes up to four
  chained calls into the destination instead, with no copies.
- **A call's answer.** A call returning a `long` or a `float` hands it
  back in E:HL, which is stored to a scratch slot at once, `ld (ix+s),hl;
  ld a,e; ld (ix+s+3),a`. Stored straight on into a variable, `r = f();`,
  it goes there instead: [`materialise_long()`](../src/wide.c#L286)
  writes the variable's displacement over the scratch slot's in those two
  stores, where copying the slot through IY was twelve bytes more. `return
  f();` takes the stores back ([`gen_return()`](../src/func.c#L814)) and
  returns E:HL as it came.
- **An int made a `long`** has its top byte filled with its sign, `push
  hl; add hl,hl; pop hl; sbc a,a`. A value known to fit a byte or two
  (section 2) -- an unsigned char read and widened, an unsigned short, an
  int ANDed with a byte mask -- is not negative whatever its type, and its
  top byte is `xor a` ([`materialise_long()`](../src/wide.c#L286)).
- **Three bytes at a time through IY.** A `long` copies as `ld
  iy,(ix+s); ld (ix+d),iy` plus a byte, 12 bytes against 24
  ([`copy_long()`](../src/wide.c#L74)); a constant is written three bytes at a time
  ([`wide_bytes_at()`](../src/wide.c#L448)), and the load into IY is skipped when the
  next three bytes are the same.
- **Float negation** is `xor 80h` on the top byte. `f - c` for a constant
  `c` is `f + (-c)`, so the pool can hold it.
- **The float routines take a fast path** when both operands and the
  result are normal numbers, which is nearly every call: `fadd`, `fmul` and
  `fdiv` read the exponents from the operands' top bytes and work in
  registers, with the multiply's product summed a column at a time from
  MLTs and the divide's quotient gathered a byte at a time in A. They share
  one rounding and packing routine, `acc_rt_fround`. Anything else -- a
  zero, a denormal, an infinity, a NaN, a result past either end -- goes to
  the general code, which unpacks both operands and handles every case.
  `fcmp` compares the bytes as they are and writes neither operand, so a
  float compare reads its right operand where it lies or from the pool.
  Every result is checked bit for bit against the host's IEEE arithmetic
  by `test/floatrt.sh`.
- **The runtime's division** runs its shift-and-subtract loop entirely in
  registers, starting from the dividend's first nonzero byte: 24 turns for
  a value under 2²⁴, 16 under 2¹⁶. The 64-bit multiply skips zero rows, and
  the 64-bit divide has a fast path for a one-byte divisor.

## 10. Jumps

**Short jumps.** Every jump is emitted as a 3-byte `jp` through
[`jump_op()`](../src/branch.c#L76) and recorded. At the end of each function,
[`relax_function()`](../src/relax.c#L793) shortens every one whose condition has a `jr`
form (always, Z, NZ, C, NC) and whose target is in reach, cutting the two
bytes. Reach is judged before anything shrinks, and shrinking only brings
targets closer, so one pass is enough. This saves about 4% of an image.
Doing it once per function rather than once per file keeps its cost small
and its tables short.

**A jump over a jump.** `jp cc,L1; jp L2; L1:` becomes `jp !cc,L2`
([`branch_over()`](../src/relax.c#L634)) when nothing else jumps to the second jump. That
is every `if (x) break;`, `if (x) continue;`, and a `return` that jumps to
an earlier return of the same constant. A jump to the next instruction,
whatever its condition, is taken out the same way: the end of a `?:`
arm jumping past an arm that is nothing, a switch's last test.

**Loops.** A `for` loop is test, body, step, and one jump back to the
test: its step is recorded as text and compiled after the body
([`step_again()`](../src/stmt.c#L844)). `do … while` is one conditional jump back. A
backward jump is recorded so that it can be shortened too
([`gen_jump_to()`](../src/branch.c#L161)).

**`switch`** tests its value after the body, loaded once
([`gen_switch_load()`](../src/branch.c#L613)). [`switch_statement()`](../src/stmt.c#L544)
counts the cases a value of the type can meet, and the lowest and highest
of them, and the case is found whichever of three ways is smallest, the
faster where two are as small:

- **By index**, for cases that lie close together, on a char or an int: a
  call to `acc_rt_jump8` or `acc_rt_jump` followed by the lowest case's
  value and how many values there are from it to the highest, then an
  address for each of those values and one more, three bytes each. A
  value no case has, or one outside them, takes the address past the
  table, where the default is. The case is found at once, with no test
  for each:

  ```
  ld   hl,(ix-3)
  call acc_rt_jump
  db   5              ; the count
  dl   1              ; the lowest
  dl   case1, case2, case3, case4, case5, none
  ```

- **One at a time**, a comparison and a jump each. On an int each case is
  `ld de,v; or a; sbc hl,de; add hl,de; jp z,case`
  ([`gen_switch_case()`](../src/branch.c#L657)), twelve bytes -- the `add`
  puts HL back and leaves Z alone, so the value is never reloaded. A `long`
  is always compared this way, its top byte in A first, the rest skipped on
  a mismatch.
- **By search**, on an int with two cases or more: a call to
  `acc_rt_switch` followed by a count and each case's value and address,
  six bytes a case, which the routine reads along, passing over an entry
  on its low byte and comparing the whole value only where that is the
  same. Not on a char, where its entries would be four bytes against the
  comparisons' six: over the five programs of `test/apps.sh` that saved a
  twentieth of a percent of their code for half a percent more of their
  cycles.

A table's addresses are relocated, and the short-jump pass moves them with
the code. A switch on a char keeps it a byte: it is loaded into A, compared
with `cp n; jp z,case` or used as the table's index, and a case the char
cannot be has no test and no entry. A char just read through a pointer is
held as that byte too ([`vbyte_held()`](../src/branch.c#L328)), so `switch
(*p++)` in a lexer works on the byte.

## 11. Functions

**The prologue.** Written out it is `push ix; ld ix,0; add ix,sp; lea
hl,ix-frame; ld sp,hl`, 13 bytes, or `ld hl,-frame; add hl,sp; ld sp,hl`
for a frame past `(ix+d)`'s reach
([`gen_func_begin()`](../src/func.c#L472)). A function with no locals
keeps the first nine. A call to a frame-setting routine, `ld hl,-frame;
call acc_rt_frameset` as agondev makes to `__frameset`, is 8 bytes, but
some 15 cycles more on every entry: with every prologue written out, acc
compiling its own inputs runs 5.2% faster, AED 4.4%, ez80asm 3.1% and zap
2.7%, for 1-2% more code. So the choice is made at the end of the
function, when its length is known: under 200 bytes the prologue stays
written out, and a longer function, whose entry is a smaller share of what
it costs, has it made the call ([`frame_to_call()`](../src/func.c#L636)),
or `call acc_rt_frameset0` with no frame. Over the five programs of
`test/apps.sh` that is 0.7% less code for 1.4% more cycles.

**No frame where none is needed.** A function with no locals has SP where
its prologue left it at every return, so its epilogue is `pop ix; ret`.
Where its code never reads IX either, it has no frame at all: the
prologue is cut and the epilogue is `ret`, so a static read is `ld
hl,(nn); ret` ([`gen_func_end()`](../src/func.c#L658)).

**One epilogue.** Every `return` jumps to one `ld sp,ix; pop ix; ret` at
the end of the function. A jump is two bytes once shortened and the
epilogue five, and zap's functions average 4.5 returns each. A `return`
that would jump to the next byte is taken back.

**Constant returns shared.** A second `return 0;` jumps to the first one's
`ld hl,0` rather than repeating it ([`gen_return()`](../src/func.c#L814)): 2 bytes against
9, for up to eight constants a function.

**Local arrays** take their address as `push de; ld de,K; push ix; pop hl;
add hl,de; pop de` while the frame's size is unknown. At the end,
[`gen_func_end()`](../src/func.c#L658) rewrites every one within reach as `lea hl,ix+d`
and cuts the other seven bytes.

**Built-in calls.** `memcpy`, `memmove`, `memset` and `memchr` are calls to
runtime routines built on `ldir` and `cpir`, with their operands in
registers ([`mem_builtin()`](../src/func.c#L1080)); a byte loop in C cost zap 7% of its
time. `ldir` reads a count of zero as 16 MB, so each routine tests for
none in place, `or a; sbc hl,hl; sbc hl,bc`, rather than through a call:
a thousand 7-byte fills and copies take 227 thousand cycles so, against
287 thousand with the test called. `exit` is four instructions in line ([`exit_builtin()`](../src/func.c#L1010)).
Struct copies are `ldir`.

**Char arguments.** An argument for a parameter declared `char`, `signed
char` or `unsigned char` is pushed as the byte, with whatever lies above
it in the slot ([`narrow_argument()`](../src/func.c#L1357)): the callee
reads only the low byte, `ld a,(ix+d)`, and widens it itself, as agondev's
callees do too. So there is no sign or zero fill, four or five bytes,
before the push: a byte just read has its widening taken back, `ld a,(hl);
ld l,a; push hl`, and a char local is pushed with the two bytes past it,
`ld hl,(ix+d); push hl`. Not a `_Bool`, and not an argument past a
prototype's parameters, which C promotes to `int`.

**Byte results** are returned in A as well as HL, and the caller treats A
as just loaded (section 5), so `if (f())` tests A.

## 12. Inline functions

A call costs about 60 cycles before the callee does anything, and zap
calls its character-class tests hundreds of thousands of times. A `static
inline` function whose body is one `return` of an int-or-narrower value,
with at most eight int-or-narrower parameters and no varargs, is expanded
at each call: its text is recorded as it is compiled
([`return_kept()`](../src/inline.c#L253)), and [`inline_expand()`](../src/inline.c#L312) stores the
arguments in scratch slots and parses the text again with the parameter
names bound to them. Every other name in the text must still mean what it
meant at the definition, or the call is made normally. Nesting stops at
four, which also stops recursion.

## 13. Only what is used

**Unused static functions.** A `static` or `static inline` function that
nothing outside the unused ones calls or takes the address of is cut out
at the end of the file ([`drop_unused_statics()`](../src/relax.c#L975)), working outward
from what is used so that mutually recursive dead functions go too. A
header of `static inline` helpers costs a file only the ones it calls.

**Runtime routines.** The runtime is a library, `rt.a`, one object for each
routine or group of routines that share code ([`lib/rt/`](../lib/rt)), and
a link takes the objects a program calls and no others. One multiply does
not bring the float routines with it.

**Zero-initialised globals** go in the bss, after the image: they take no
room on the card, and are cleared by the startup with `ld (hl),0` and an
`ldir` ([`bss_emit()`](../src/finish.c#L455)). A one-byte bss is cleared without the
`ldir`, which with a count of zero would clear 16 MB.

**Library members.** A link takes from a library only the items -- single
functions and objects -- that are wanted, and what they reach
([`take_items()`](../src/link.c#L253)).

## 14. Smaller things

- A short narrows through IY, so DE is not disturbed.
- A local array is zeroed by clearing one byte and copying it along with
  `ldir`.
- A call through a pointer loads IY straight from a constant or a frame
  slot.
- A string literal inside a function is laid down where it is used and
  jumped over, so its address is a constant from the moment it is written.
