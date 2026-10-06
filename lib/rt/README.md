# acc's runtime

What the code acc generates calls for the operations the eZ80 has no
instruction for, and what C cannot write at all. They are eZ80 assembly,
assembled by [zap](https://github.com/avalonbits/zap) into acc's object
format and put in `rt.a`, a library beside `libc.a` (`/lib/acc/rt.a` on the
Agon), where a link takes them like any other member: a program carries the
ones it calls and nothing else. A link reads `rt.a` first, since every
program calls it and its index is short, then `libc.a` only if a name is
still waiting, and `rt.a` again for what the C library's members call.

- **Operators.** A 24-bit AND is not an instruction: AND is eight bits wide
  and the upper byte of HL has no name. Shifts are a loop over the bits,
  since there is no barrel shifter. Multiply has MLT, which is 8x8, and
  divide has nothing. So `&`, `|`, `^`, `<<`, `>>`, `*`, `/` and `%` on ints
  and on the narrower types are calls here, and every operation on a `long`,
  a `long long` and a `float`.
- **The frame.** `acc_rt_frameset` and `acc_rt_frameset0`, which every
  function's prologue calls.
- **Block moves.** `memcpy`, `memmove`, `memset` and `memchr`, which acc calls
  in place of the library's where a program names them.
- **The machine.** MOS calls, the eZ80's I/O ports, printing, `setjmp` and
  `longjmp`, and the handler kbuf installs for MOS to call on each key.

## Calling convention

The helpers the code generator calls take the left operand in HL and the
right in BC, and answer in HL. Everything else is kept: acc's register
allocator may have live values in DE, and IY is its scratch. A `long`, a
`long long` or a `float` lives in the frame; the routine takes the address
of the destination and left operand in HL and of the right in DE.

The routines named `_acc_rt_lr...` take a `long` in registers instead, as
agondev's do: the left operand, and the answer, in E:UHL -- E the top byte,
HL the low three -- and the right in A:UBC, or for a shift its count in A.
A comparison answers in the flags, carry where the left is the less and Z
where they are equal, and changes neither. They keep BC, D, IX and IY; A
and the flags they do not. `test/rtlong.sh` holds each to acc's long
operators.

What is named as C names it (`_setjmp`, `_longjmp`) is a C function: its
arguments are three bytes each from `(sp+3)`, the answer is in HL, and only
IX is kept. The names that start `_acc_rt_` are not C's to call.

## One file, one object

A link takes an object whole, so each file is what a program either needs
all of or none of: one routine, or several that share code. A routine that
calls into another file's names it with `XREF`, and that file exports the
label with `XDEF`. The files were cut where the routines stop sharing code,
so that a program that multiplies once carries the multiply and not the
floating-point routines.

zap takes the GNU spellings of its directives (`.global`, `.section`) as well
as its own, and `test/rtlib.sh` checks that every file assembles and that
what each defines is in `rt.a`.

## The licence

These files are LGPL-2.1-or-later with an exception, which each file names
as `AdditionRef-acc-runtime-exception`:

> In addition to the permissions in the GNU Lesser General Public License,
> the author gives you unlimited permission to link the compiled version of
> this file with other programs, and to distribute those programs without any
> restriction coming from the use of this file. (The GNU Lesser General
> Public License restrictions do apply in other respects; for example, they
> cover modification of the file, and distribution when not linked into
> another program.)
>
> Note that people who make modified versions of this file are not obligated
> to grant this special exception for their modified versions; it is their
> choice whether to do so. The GNU Lesser General Public License gives
> permission to release a modified version without this exception; this
> exception also makes it possible to release a modified version which
> carries forward this exception.

The exception is glibc's, from its startup code, and is here for the same
reason: every program acc compiles carries some of this code, and a program
does not take on acc's licence by being compiled by it.
