;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; acc is free software; you can redistribute it and/or modify it under the
; terms of the GNU Lesser General Public License as published by the Free
; Software Foundation; either version 2.1 of the License, or (at your option)
; any later version.
;
; In addition to the permissions in the GNU Lesser General Public License,
; the author gives you unlimited permission to link the compiled version of
; this file with other programs, and to distribute those programs without any
; restriction coming from the use of this file. (The GNU Lesser General Public
; License restrictions do apply in other respects; for example, they cover
; modification of the file, and distribution when not linked into another
; program.)
;
; Note that people who make modified versions of this file are not obligated
; to grant this special exception for their modified versions; it is their
; choice whether to do so. The GNU Lesser General Public License gives
; permission to release a modified version without this exception; this
; exception also makes it possible to release a modified version which carries
; forward this exception.
;
; The exception is glibc's, from its startup code, and is here for the same
; reason: acc copies this code into every program it compiles, and a program
; does not take on acc's license by being compiled by it.
;
; The operators the eZ80 has no instruction for.
;
; A 24-bit AND is not an instruction: AND is eight bits wide and the upper
; byte of HL has no name, so two thirds of the register can be reached and the
; rest cannot. Shifts are worse -- there is no barrel shifter, so a shift is a
; loop over the bits whatever the amount. Multiply has MLT, which is 8x8, and
; divide has nothing at all.
;
; agondev's compiler calls into libagon for all of this. acc has nothing to
; link against, so it carries these and emits the ones a program uses into
; that program's image. Each is self-contained: none calls another, so the
; ones a program does not use cost it nothing and the ones it does need no
; relocation beyond the call itself.
;
; The convention is the one agondev uses for the same operations, so that the
; two could be mixed later: left operand in HL, right in BC, result in HL.
; Everything else is preserved -- acc's register allocator may have live
; values in DE, and IY is its scratch.
;
; The upper byte is reached through the stack. Pushing HL puts all three bytes
; somewhere addressable, and (iy+d) can then get at the one that has no name.
;
	.assume adl=1
	.section .text,"ax",@progbits

; ================================================================ the frame
; The prologue every function begins with, as agondev's __frameset: called
; with HL the frame's size, negated, it saves IX, points it at the saved
; IX, and makes room for the frame below. The return address is taken off
; first and jumped to at the end, so the frame starts where the caller's
; call left SP. HL and DE go; nothing is in them at a function's entry.
; frameset0 is the same for a function with no frame. First in the blob,
; and acc_rt_machine marks where they end: every program carries these, and
; only a program that uses something below carries more.

	.global _acc_rt_frameset
	.global _acc_rt_frameset0

_acc_rt_frameset:
	pop de
	push ix
	ld ix, 0
	add ix, sp
	add hl, sp
	ld sp, hl
	ex de, hl
	jp (hl)

_acc_rt_frameset0:
	pop de
	push ix
	ld ix, 0
	add ix, sp
	ex de, hl
	jp (hl)

	.global	acc_rt_machine
acc_rt_machine:

; ================================================================ the machine
; What the eZ80 cannot do on its own but MOS can, which is everything to do
; with the world outside the program. First in the blob, and acc_rt_ops marks
; where they end: a program that only prints carries these and not the four
; kilobytes of arithmetic behind them.
;
; Called from C and not by the code generator, so the argument is where acc
; puts one -- three bytes at (sp+3), past the return address -- rather than
; in a register. What comes back comes back in HL, as from any function acc
; compiles.

	.global _acc_rt_putch

; int acc_rt_putch(int c): the low byte of c to the console, and c back.
_acc_rt_putch:
	ld	hl, 3
	add	hl, sp
	ld	a, (hl)
	ld	hl, 0
	ld	l, a
	push	hl
	rst.lil	$10
	pop	hl
	ret

	.global _acc_rt_mos
	.global _acc_rt_mos_de
	.global _acc_rt_mos_hl
	.global _acc_rt_mos_ix
	.global _acc_rt_puts

; A MOS call: the number in A, and HL, DE and BC as that call wants them.
;
; Four ways out, because MOS answers in whichever register suits what was
; asked -- an error code in A, a count of bytes in DE, a pointer in HL, the
; system variables in IX -- and a C function answers in HL.
;
; The arguments are where acc leaves them, which is where agondev leaves them
; too: three bytes each, the first at (ix+6) once ix is the stack pointer and
; the saved ix and the return address are behind it. IX is saved and put back
; because MOS is free with it, which is what libagon does here as well.

_acc_rt_mos:
	push	ix
	ld	ix, 0
	add	ix, sp
	ld	hl, (ix+9)
	ld	de, (ix+12)
	ld	bc, (ix+15)
	ld	a, (ix+6)
	rst.lil	$08
	ld	hl, 0
	ld	l, a
	pop	ix
	ret

_acc_rt_mos_de:
	push	ix
	ld	ix, 0
	add	ix, sp
	ld	hl, (ix+9)
	ld	de, (ix+12)
	ld	bc, (ix+15)
	ld	a, (ix+6)
	rst.lil	$08
	ex	de, hl
	pop	ix
	ret

_acc_rt_mos_hl:
	push	ix
	ld	ix, 0
	add	ix, sp
	ld	hl, (ix+9)
	ld	de, (ix+12)
	ld	bc, (ix+15)
	ld	a, (ix+6)
	rst.lil	$08
	pop	ix
	ret

; The system variables come back in IX, which has to be got out from under
; the saved one: push what MOS gave, take it into HL, then put the old one
; back.
_acc_rt_mos_ix:
	push	ix
	ld	ix, 0
	add	ix, sp
	ld	hl, (ix+9)
	ld	de, (ix+12)
	ld	bc, (ix+15)
	ld	a, (ix+6)
	rst.lil	$08
	push	ix
	pop	hl
	pop	ix
	ret

; Writing a run of bytes is a different restart: the buffer in HL, how many
; in BC, and the byte it stops at in A.
_acc_rt_puts:
	push	ix
	ld	ix, 0
	add	ix, sp
	ld	hl, (ix+6)
	ld	bc, (ix+9)
	ld	a, (ix+12)
	rst.lil	$18
	pop	ix
	ret

; Copying and filling, which the eZ80 does in one instruction where C does
; it a byte at a time. acc calls these where a program says memcpy, memmove
; or memset: the library still has its own, for a program that takes their
; address, but nothing reaches them by name any more.
;
; ldir and lddr read the count as the full twenty-four bits of BC, so a count
; of nothing copies sixteen megabytes rather than nothing at all. The guard
; is here and not at the call, because here it is written once.
;
;	hl = source, de = destination, bc = how many
;	returns hl = the destination, which is what memcpy answers

	.global _acc_rt_memcpy
	.global _acc_rt_memmove
	.global _acc_rt_memset
	.global _acc_rt_memchr

_acc_rt_memcpy:
	push	de			; the answer
	call	_rt_bc_is_zero
	jr	z, _rt_copy_done
	ldir
_rt_copy_done:
	pop	hl
	ret

; The same, but right to left when the blocks overlap the wrong way round.
; Overlapping forwards is what ldir already does correctly.
_acc_rt_memmove:
	push	de
	call	_rt_bc_is_zero
	jr	z, _rt_copy_done
	push	hl			; source below destination: go backwards
	sbc	hl, de			; carry is clear, from bc_is_zero
	pop	hl
	jr	nc, _rt_move_up
	add	hl, bc			; both ends, one past the last byte
	dec	hl
	ex	de, hl
	add	hl, bc
	dec	hl
	ex	de, hl
	lddr
	pop	hl
	ret
_rt_move_up:
	ldir
	pop	hl
	ret

;	hl = where, a = the byte, bc = how many
;	returns hl = where, which is what memset answers
_acc_rt_memset:
	push	hl
	call	_rt_bc_is_zero
	jr	z, _rt_fill_done
	ld	(hl), a			; the first one by hand, then copy it
	dec	bc			; along, which is how ldir fills
	call	_rt_bc_is_zero
	jr	z, _rt_fill_done
	push	hl
	pop	de
	inc	de
	ldir
_rt_fill_done:
	pop	hl
	ret

; Looking for a byte, which cpir does in one instruction: compare A with
; what HL points at, step HL on, count BC down, and stop at the first match
; or when the count runs out. It leaves HL one past what it found, and Z set
; only if it found it.
;
;	hl = where to look, a = the byte, bc = how many
;	returns hl = the byte, or nothing at all
_acc_rt_memchr:
	call	_rt_bc_is_zero
	jr	z, _rt_chr_none
	cpir
	jr	nz, _rt_chr_none
	dec	hl			; cpir stepped past what it matched
	ret
_rt_chr_none:
	ld	hl, 0
	ret

; Z when BC is nothing. Clears the carry either way, which memmove reads.
; HL is wanted by all three callers, so the test goes through the stack
; rather than through it.
_rt_bc_is_zero:
	push	hl
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	pop	hl
	ret

;---------------------------------------------------------------- MOS calls
; int acc_rt_mos_call(AccMosRegs *r): a MOS call with every register it may
; want, and every register it may answer in, through one structure:
;
;	+0  HL   +3  DE   +6  BC   +9  IX   +12  IY   (three bytes each)
;	+15 A, the call's number going in and what MOS answers coming out
;	+16 the carry flag MOS left, 0 or 1
;
; Some calls take IX and IY as arguments and some answer in them, so both
; are loaded from the structure and stored back to it; the caller's own are
; kept on the stack meanwhile, IX being its frame. What comes back in HL is
; A, which is what most calls answer with.

	.global _acc_rt_mos_call

_acc_rt_mos_call:
	push	ix
	push	iy
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy+9)		; r
	push	hl			; kept, to store the answers through
	push	hl
	pop	iy
	ld	hl, (iy+0)
	ld	de, (iy+3)
	ld	bc, (iy+6)
	ld	ix, (iy+9)
	ld	a, (iy+15)
	ld	iy, (iy+12)
	rst.lil	$08
	ex	(sp), iy		; iy = r, and MOS's iy kept; flags untouched
	ld	(iy+16), 0		; nor does this touch them
	jr	nc, _rt_mos_nc
	ld	(iy+16), 1
_rt_mos_nc:
	ld	(iy+0), hl
	ld	(iy+3), de
	ld	(iy+6), bc
	ld	(iy+9), ix
	ld	(iy+15), a
	pop	hl			; MOS's iy
	ld	(iy+12), hl
	ld	hl, 0
	ld	l, a
	pop	iy
	pop	ix
	ret

; int acc_rt_port_in(int port) and void acc_rt_port_out(int port, int value):
; the eZ80's own I/O space, where the GPIO ports and the timers are, which
; C has no way to reach. The port is the whole of BC, as IN r,(C) takes it.

	.global _acc_rt_port_in
	.global _acc_rt_port_out

_acc_rt_port_in:
	ld	iy, 0
	add	iy, sp
	ld	bc, (iy+3)
	in	a, (c)
	ld	hl, 0
	ld	l, a
	ret

_acc_rt_port_out:
	ld	iy, 0
	add	iy, sp
	ld	bc, (iy+3)
	ld	a, (iy+6)
	out	(c), a
	ret

;---------------------------------------------------------------- setjmp
; setjmp and longjmp, which C cannot write: they are the stack pointer and
; the frame pointer, put back. Named as C names them rather than as acc_rt_
; helpers, since a program calls them itself; see embed.py.
;
; A jmp_buf is three words: where setjmp was called from, IX -- the one
; register a callee must give back, so the only one the caller can have
; anything in across the call -- and the stack pointer as it was at the
; call, pointing at the return address. Everything else a function holds
; across a call it holds in its frame, and the frame is where IX says.
;
; longjmp puts IX and the stack back, drops the return address as `ret`
; would have, and goes to it with the value -- or 1 for a 0, since C says
; setjmp may not appear to return 0 twice.

	.global _setjmp
	.global _longjmp

; int setjmp(jmp_buf env)
_setjmp:
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy+3)		; env
	ld	de, (iy+0)		; where to come back to
	ld	(hl), de
	inc	hl
	inc	hl
	inc	hl
	ld	(hl), ix
	inc	hl
	inc	hl
	inc	hl
	ld	(hl), iy		; the stack, at the return address
	ld	hl, 0			; and 0, the first time
	ret

; void longjmp(jmp_buf env, int val)
_longjmp:
	ld	iy, 0
	add	iy, sp
	ld	de, (iy+6)		; val
	ld	hl, (iy+3)		; env
	ld	bc, (hl)		; where setjmp was called from
	inc	hl
	inc	hl
	inc	hl
	ld	ix, (hl)
	inc	hl
	inc	hl
	inc	hl
	ld	hl, (hl)
	ld	sp, hl
	inc	sp			; past the return address, as ret leaves it
	inc	sp
	inc	sp
	ex	de, hl
	ld	de, 0
	or	a, a
	sbc	hl, de
	jr	nz, _rt_jmp_go
	inc	hl
_rt_jmp_go:
	push	bc
	ret

; ================================================================ arithmetic
	.global	acc_rt_ops
acc_rt_ops:

	.global _acc_rt_and
	.global _acc_rt_or
	.global _acc_rt_xor
	.global _acc_rt_shl
	.global _acc_rt_shru
	.global _acc_rt_shrs
	.global _acc_rt_mul

; ---------------------------------------------------------------- bitwise
; hl = hl OP bc, a byte at a time, with both operands on the stack so that
; every byte of them has an address.

_acc_rt_and:
	push	de
	push	iy
	push	bc			; (iy+3..5) once iy is set
	push	hl			; (iy+0..2)
	ld	iy, 0
	add	iy, sp
	ld	a, (iy + 0)
	and	a, (iy + 3)
	ld	(iy + 0), a
	ld	a, (iy + 1)
	and	a, (iy + 4)
	ld	(iy + 1), a
	ld	a, (iy + 2)
	and	a, (iy + 5)
	ld	(iy + 2), a
	pop	hl
	pop	bc
	pop	iy
	pop	de
	ret

_acc_rt_or:
	push	de
	push	iy
	push	bc
	push	hl
	ld	iy, 0
	add	iy, sp
	ld	a, (iy + 0)
	or	a, (iy + 3)
	ld	(iy + 0), a
	ld	a, (iy + 1)
	or	a, (iy + 4)
	ld	(iy + 1), a
	ld	a, (iy + 2)
	or	a, (iy + 5)
	ld	(iy + 2), a
	pop	hl
	pop	bc
	pop	iy
	pop	de
	ret

_acc_rt_xor:
	push	de
	push	iy
	push	bc
	push	hl
	ld	iy, 0
	add	iy, sp
	ld	a, (iy + 0)
	xor	a, (iy + 3)
	ld	(iy + 0), a
	ld	a, (iy + 1)
	xor	a, (iy + 4)
	ld	(iy + 1), a
	ld	a, (iy + 2)
	xor	a, (iy + 5)
	ld	(iy + 2), a
	pop	hl
	pop	bc
	pop	iy
	pop	de
	ret

; ---------------------------------------------------------------- shifts
; hl = hl shifted by c places. C leaves a shift of the width or more
; undefined; the count is masked to five bits so the loop terminates, and a
; count of 24 or more then shifts every bit out, which is a defensible answer.

_acc_rt_shl:
	ld	a, c
	and	a, 31
	ret	z
.shl_loop:
	add	hl, hl
	dec	a
	jr	nz, .shl_loop
	ret

; Right shifts go through the stack: there is no way to shift HL right as
; three bytes, but (iy+d) reaches each of them in turn.

_acc_rt_shru:
	ld	a, c
	and	a, 31
	ret	z
	push	iy
	push	hl
	ld	iy, 0
	add	iy, sp
.shru_loop:
	srl	(iy + 2)
	rr	(iy + 1)
	rr	(iy + 0)
	dec	a
	jr	nz, .shru_loop
	pop	hl
	pop	iy
	ret

_acc_rt_shrs:
	ld	a, c
	and	a, 31
	ret	z
	push	iy
	push	hl
	ld	iy, 0
	add	iy, sp
.shrs_loop:
	sra	(iy + 2)		; the sign is carried down, not zero
	rr	(iy + 1)
	rr	(iy + 0)
	dec	a
	jr	nz, .shrs_loop
	pop	hl
	pop	iy
	ret

; ---------------------------------------------------------------- multiply
; hl = hl * bc, twenty-four bits, wrapping.
;
; MLT is the only multiplier and it is 8x8 -> 16, so a 24-bit product is
; built from the partial products of the bytes. Only the low three bytes of
; the result are kept, so the pairs that land entirely above bit 23 are not
; computed at all: with the operands as l,h,u and c,b,y that leaves
;
;   l*c          the low pair, contributing to bytes 0 and 1
;   l*b + h*c    contributing to bytes 1 and 2
;   l*y + h*b + u*c   contributing to byte 2 only
;
; Six multiplies rather than the twenty-four iterations a shift-and-add loop
; would take.

_acc_rt_mul:
	push	de
	push	iy
	push	bc			; (iy+3..5): the right operand
	push	hl			; (iy+0..2): the left
	ld	iy, 0
	add	iy, sp

	; byte 2 of the result: the three pairs that reach it, low byte only
	ld	b, (iy + 0)		; l
	ld	c, (iy + 5)		; y
	mlt	bc
	ld	a, c
	ld	b, (iy + 1)		; h
	ld	c, (iy + 4)		; b
	mlt	bc
	add	a, c
	ld	b, (iy + 2)		; u
	ld	c, (iy + 3)		; c
	mlt	bc
	add	a, c
	ld	e, a			; e = byte 2 so far

	; bytes 1 and 2: l*b + h*c, both sixteen-bit
	ld	b, (iy + 0)
	ld	c, (iy + 4)
	mlt	bc
	ld	d, b			; keep the high half
	ld	a, c
	ld	b, (iy + 1)
	ld	c, (iy + 3)
	mlt	bc
	add	a, c			; a = byte 1 so far
	ld	c, a
	ld	a, d
	adc	a, b			; carries into byte 2
	add	a, e
	ld	e, a			; e = byte 2
	ld	d, c			; d = byte 1

	; bytes 0 and 1: l*c
	ld	b, (iy + 0)
	ld	c, (iy + 3)
	mlt	bc
	ld	a, b
	add	a, d			; byte 1
	ld	d, a
	jr	nc, .mul_no_carry
	inc	e
.mul_no_carry:
	; assemble: c = byte 0, d = byte 1, e = byte 2
	ld	(iy + 0), c
	ld	(iy + 1), d
	ld	(iy + 2), e
	pop	hl
	pop	bc
	pop	iy
	pop	de
	ret

; ---------------------------------------------------------------- divide
; There is no divide instruction at all, so this is the long way: shift the
; dividend into a remainder a bit at a time and subtract the divisor whenever
; it fits. Twenty-four iterations.
;
; The signed forms reduce to the unsigned one. C99 requires division to
; truncate towards zero and the remainder to take the sign of the dividend,
; which is what taking both magnitudes and fixing the sign afterwards gives.
;
; Dividing by zero is undefined in C. These return zero rather than looping.

	.global _acc_rt_divu
	.global _acc_rt_remu
	.global _acc_rt_divs
	.global _acc_rt_rems

; hl = hl / bc, de = hl % bc, both unsigned. The common core.
_acc_rt_udivmod:
	call	_rt_bc_is_zero		; all twenty-four bits of it: `ld a, b`
	jr	nz, .div_go		; with `or a, c` reads only sixteen, and
	ld	hl, 0			; called every divisor that is a multiple
	ld	de, 0			; of 65536 nothing at all
	ret
.div_go:
	ld	de, 0			; the remainder
	ld	a, 24
.div_loop:
	add	hl, hl			; the quotient shifts in at the bottom
	ex	de, hl
	adc	hl, hl			; remainder = remainder * 2 + the bit out
	; That shift cannot carry out of twenty-four bits. Before round i the
	; remainder holds the top i-1 bits of the dividend taken modulo the
	; divisor, so it is below 2^(i-1); at the last round that is below
	; 2^23, and twice it still fits. So the carry is clear here and the
	; subtract below needs nothing to clear it.
	sbc	hl, bc
	jr	nc, .div_fits
	add	hl, bc			; it did not fit: put the remainder back
	ex	de, hl
	jr	.div_next
.div_fits:
	ex	de, hl
	inc	l			; the bit fits, so record it. L is even
					; here, so this cannot carry into H
.div_next:
	dec	a
	jr	nz, .div_loop
	ret

_acc_rt_divu:
	push	de
	call	_acc_rt_udivmod
	pop	de
	ret

_acc_rt_remu:
	push	de			; the caller may have a live value there
	call	_acc_rt_udivmod
	ex	de, hl			; the remainder is the answer
	pop	de
	ret

; The signed forms reduce to the unsigned one: divide the magnitudes, then
; fix the sign. C99 requires the quotient to truncate towards zero and the
; remainder to take the sign of the dividend, which is what that gives.
;
; The sign of a 24-bit value is bit 23, which lives in the byte of HL that has
; no name -- but `add hl, hl` shifts it into the carry, and a push and a pop
; either side of that leaves the value untouched.

_acc_rt_divs:
	push	de
	push	iy
	ld	e, 0			; the sign of the result, in bit 0

	push	hl
	add	hl, hl
	pop	hl			; carry = the dividend is negative
	jr	nc, .divs_num_ok
	ld	e, 1
	push	bc
	push	hl
	pop	bc
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	pop	bc
.divs_num_ok:
	push	hl
	push	bc
	pop	hl
	add	hl, hl
	pop	hl			; carry = the divisor is negative
	jr	nc, .divs_den_ok
	ld	a, e
	xor	a, 1
	ld	e, a
	push	hl
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	push	hl
	pop	bc
	pop	hl
.divs_den_ok:
	push	de			; udivmod returns the remainder in de
	call	_acc_rt_udivmod
	pop	de
	bit	0, e
	jr	z, .divs_done
	push	bc
	push	hl
	pop	bc
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	pop	bc
.divs_done:
	pop	iy
	pop	de
	ret

_acc_rt_rems:
	push	de
	push	iy
	ld	e, 0			; the sign of the dividend, in bit 0

	push	hl
	add	hl, hl
	pop	hl
	jr	nc, .rems_num_ok
	ld	e, 1
	push	bc
	push	hl
	pop	bc
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	pop	bc
.rems_num_ok:
	push	hl
	push	bc
	pop	hl
	add	hl, hl
	pop	hl
	jr	nc, .rems_den_ok
	push	hl			; the divisor's sign does not reach the
	ld	hl, 0			; remainder, but its magnitude is needed
	or	a, a
	sbc	hl, bc
	push	hl
	pop	bc
	pop	hl
.rems_den_ok:
	push	de
	call	_acc_rt_udivmod
	ex	de, hl			; the remainder is the answer
	pop	de
	bit	0, e
	jr	z, .rems_done
	push	bc
	push	hl
	pop	bc
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	pop	bc
.rems_done:
	pop	iy
	pop	de
	ret

; ---------------------------------------------------------------- long
; A long is four bytes, wider than any register, so it lives in the frame and
; these work on it there: HL points at the destination, DE at the other
; operand, and the destination is overwritten.
;
; Little endian, so the loop runs from the low byte up and the carry chains
; the way the arithmetic needs.

	.global _acc_rt_ladd
	.global _acc_rt_lsub
	.global _acc_rt_land
	.global _acc_rt_lor
	.global _acc_rt_lxor
	.global _acc_rt_lcmpeq
	.global _acc_rt_lcmpord

_acc_rt_ladd:
	push	bc
	push	de
	push	hl
	ld	b, 4
.ladd_n:				; b bytes: the long long one comes in here
	or	a, a			; no carry into the low byte
.ladd_loop:
	ld	a, (de)
	adc	a, (hl)
	ld	(hl), a
	inc	hl
	inc	de
	djnz	.ladd_loop
	pop	hl
	pop	de
	pop	bc
	ret

; Only (hl) has an `sbc a, (rr)` form, so the right-hand byte goes through C
; on the way. Neither `ld c, a` nor `ld a, (hl)` touches the carry, so the
; borrow still chains from one byte to the next.
_acc_rt_lsub:
	push	bc
	push	de
	push	hl
	ld	b, 4
.lsub_n:
	or	a, a
.lsub_loop:
	ld	a, (de)
	ld	c, a
	ld	a, (hl)
	sbc	a, c
	ld	(hl), a
	inc	hl
	inc	de
	djnz	.lsub_loop
	pop	hl
	pop	de
	pop	bc
	ret

_acc_rt_land:
	push	bc
	push	de
	push	hl
	ld	b, 4
.land_n:
.land_loop:
	ld	a, (de)
	and	a, (hl)
	ld	(hl), a
	inc	hl
	inc	de
	djnz	.land_loop
	pop	hl
	pop	de
	pop	bc
	ret

_acc_rt_lor:
	push	bc
	push	de
	push	hl
	ld	b, 4
.lor_n:
.lor_loop:
	ld	a, (de)
	or	a, (hl)
	ld	(hl), a
	inc	hl
	inc	de
	djnz	.lor_loop
	pop	hl
	pop	de
	pop	bc
	ret

_acc_rt_lxor:
	push	bc
	push	de
	push	hl
	ld	b, 4
.lxor_n:
.lxor_loop:
	ld	a, (de)
	xor	a, (hl)
	ld	(hl), a
	inc	hl
	inc	de
	djnz	.lxor_loop
	pop	hl
	pop	de
	pop	bc
	ret

; Z set if the four bytes at (hl) equal those at (de). Neither is changed.
; Compared a byte at a time and stopped at the first difference, rather than
; subtracted: a subtract's Z would describe only the byte it was done on.
_acc_rt_lcmpeq:
	push	bc
	push	de
	push	hl
	ld	b, 4
.lcmpeq_n:
.lcmpeq_loop:
	ld	a, (de)
	cp	a, (hl)
	jr	nz, .lcmpeq_differ
	inc	hl
	inc	de
	djnz	.lcmpeq_loop
	xor	a, a			; every byte matched: Z
	jr	.lcmpeq_done
.lcmpeq_differ:
	ld	a, 1
	or	a, a			; NZ, whatever the bytes were
.lcmpeq_done:
	pop	hl			; pop leaves the flags alone
	pop	de
	pop	bc
	ret

; The flags of (hl) - (de) as a four-byte subtract, for ordering. The last
; sbc leaves S, P/V and C describing the whole width, which is what the
; caller's branch sequence reads. Neither operand is changed.
_acc_rt_lcmpord:
	push	bc
	push	de
	push	hl
	ld	b, 4
.lcmpord_n:
	or	a, a
.lcmpord_loop:
	ld	a, (de)
	ld	c, a
	ld	a, (hl)
	sbc	a, c
	inc	hl
	inc	de
	djnz	.lcmpord_loop
	pop	hl			; pop leaves the flags alone
	pop	de
	pop	bc
	ret

; --------------------------------------------------- long shifts
; (hl) = (hl) shifted by the low byte of (de).
;
; The count arrives as a long because vbinop_long converts both operands, but
; only its low byte can matter: as with the 24-bit shifts the count is masked
; to five bits, so the loop terminates and a count of 32 or more shifts every
; bit out.
;
; iy points at the destination, because (iy+d) reaches all four bytes and
; there is no (hl+d).

	.global _acc_rt_lshl
	.global _acc_rt_lshru
	.global _acc_rt_lshrs

_acc_rt_lshl:
	ld	a, (de)
	and	a, 31
	ret	z
	push	bc
	push	iy
	push	hl
	pop	iy
	ld	c, a
.lshl_loop:
	sla	(iy + 0)
	rl	(iy + 1)
	rl	(iy + 2)
	rl	(iy + 3)
	dec	c
	jr	nz, .lshl_loop
	pop	iy
	pop	bc
	ret

_acc_rt_lshru:
	ld	a, (de)
	and	a, 31
	ret	z
	push	bc
	push	iy
	push	hl
	pop	iy
	ld	c, a
.lshru_loop:
	srl	(iy + 3)
	rr	(iy + 2)
	rr	(iy + 1)
	rr	(iy + 0)
	dec	c
	jr	nz, .lshru_loop
	pop	iy
	pop	bc
	ret

_acc_rt_lshrs:
	ld	a, (de)
	and	a, 31
	ret	z
	push	bc
	push	iy
	push	hl
	pop	iy
	ld	c, a
.lshrs_loop:
	sra	(iy + 3)		; the sign is carried down, not zero
	rr	(iy + 2)
	rr	(iy + 1)
	rr	(iy + 0)
	dec	c
	jr	nz, .lshrs_loop
	pop	iy
	pop	bc
	ret

; --------------------------------------------------- long multiply
; (hl) = (hl) * (de), four bytes, wrapping.
;
; The same shape as the 24-bit multiply: MLT is 8x8 -> 16, so the product is
; assembled from the partial products of the bytes, and a pair whose place
; value is already past bit 31 is not computed at all. With the operands as
; a0..a3 and b0..b3 that leaves ten of the sixteen pairs.
;
; Both operands are copied to a stack buffer first because the destination is
; the left operand: accumulating into it in place would overwrite bytes that
; later partial products still have to read. ix reaches the copies -- the left
; at ix+0..3 and the right at ix+4..7 -- and iy the destination.

	.global _acc_rt_lmul

; Add the 16-bit product of left byte \i and right byte \j into the result at
; byte \k, carrying up as far as byte 3. Four shapes rather than one with a
; count, because what the top byte drops is the point: at \k = 3 the high half
; of the product is past the width and is not added at all.
	.macro	LMUL_AT_0 i, j
	ld	b, (ix + \i)
	ld	c, (ix + 4 + \j)
	mlt	bc
	ld	a, (iy + 0)
	add	a, c
	ld	(iy + 0), a
	ld	a, (iy + 1)
	adc	a, b
	ld	(iy + 1), a
	ld	a, (iy + 2)
	adc	a, 0
	ld	(iy + 2), a
	ld	a, (iy + 3)
	adc	a, 0
	ld	(iy + 3), a
	.endm

	.macro	LMUL_AT_1 i, j
	ld	b, (ix + \i)
	ld	c, (ix + 4 + \j)
	mlt	bc
	ld	a, (iy + 1)
	add	a, c
	ld	(iy + 1), a
	ld	a, (iy + 2)
	adc	a, b
	ld	(iy + 2), a
	ld	a, (iy + 3)
	adc	a, 0
	ld	(iy + 3), a
	.endm

	.macro	LMUL_AT_2 i, j
	ld	b, (ix + \i)
	ld	c, (ix + 4 + \j)
	mlt	bc
	ld	a, (iy + 2)
	add	a, c
	ld	(iy + 2), a
	ld	a, (iy + 3)
	adc	a, b
	ld	(iy + 3), a
	.endm

	.macro	LMUL_AT_3 i, j
	ld	b, (ix + \i)
	ld	c, (ix + 4 + \j)
	mlt	bc
	ld	a, (iy + 3)
	add	a, c
	ld	(iy + 3), a
	.endm

_acc_rt_lmul:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl
	push	hl
	pop	iy			; iy -> the destination, which is also
					; the left operand
	ld	hl, -8			; eight bytes of room for the copies
	add	hl, sp
	ld	sp, hl
	push	hl
	pop	ix

	ld	a, (iy + 0)
	ld	(ix + 0), a
	ld	a, (iy + 1)
	ld	(ix + 1), a
	ld	a, (iy + 2)
	ld	(ix + 2), a
	ld	a, (iy + 3)
	ld	(ix + 3), a
	ld	a, (de)
	ld	(ix + 4), a
	inc	de
	ld	a, (de)
	ld	(ix + 5), a
	inc	de
	ld	a, (de)
	ld	(ix + 6), a
	inc	de
	ld	a, (de)
	ld	(ix + 7), a

	ld	(iy + 0), 0
	ld	(iy + 1), 0
	ld	(iy + 2), 0
	ld	(iy + 3), 0

	LMUL_AT_0 0, 0
	LMUL_AT_1 0, 1
	LMUL_AT_1 1, 0
	LMUL_AT_2 0, 2
	LMUL_AT_2 1, 1
	LMUL_AT_2 2, 0
	LMUL_AT_3 0, 3
	LMUL_AT_3 1, 2
	LMUL_AT_3 2, 1
	LMUL_AT_3 3, 0

	ld	hl, 8
	add	hl, sp
	ld	sp, hl
	pop	hl
	pop	de
	pop	bc
	pop	iy
	pop	ix
	ret

; --------------------------------------------------- long divide
; (hl) = (hl) / (de) and (de) = (hl) % (de), four bytes.
;
; The same restoring long division as the 24-bit routine, thirty-two
; iterations of it: shift a bit out of the dividend into the remainder and
; subtract the divisor whenever it fits, with the quotient growing into the
; space the dividend vacates. Both answers come back, because a caller that
; wanted the remainder would otherwise have to divide twice.
;
; The remainder needs thirty-three bits, not thirty-two. It stays below the
; divisor, so after doubling it can reach 2*(2^32-1)+1, and a divisor above
; 2^31 makes that overflow. The bit that falls out of the top is kept in b,
; and when it is set the subtract fits whatever the borrow says.
;
; The divisor is overwritten by the remainder, which is what the caller wants
; and is safe besides: vbinop_long gives each operand its own scratch slot and
; the right one is dead once the operator has been applied.
;
; Dividing by zero is undefined in C. These leave zero rather than looping.

	.global _acc_rt_ldivu
	.global _acc_rt_lremu
	.global _acc_rt_ldivs
	.global _acc_rt_lrems

; The magnitude of the four bytes at iy, and the same at ix. Flags only; the
; pointers and every other register come back unchanged.
.labs_iy:
	bit	7, (iy + 3)
	ret	z
.lneg_iy:
	ld	a, 0
	sub	a, (iy + 0)
	ld	(iy + 0), a
	ld	a, 0			; ld leaves the borrow alone
	sbc	a, (iy + 1)
	ld	(iy + 1), a
	ld	a, 0
	sbc	a, (iy + 2)
	ld	(iy + 2), a
	ld	a, 0
	sbc	a, (iy + 3)
	ld	(iy + 3), a
	ret

.labs_ix:
	bit	7, (ix + 3)
	ret	z
	ld	a, 0
	sub	a, (ix + 0)
	ld	(ix + 0), a
	ld	a, 0
	sbc	a, (ix + 1)
	ld	(ix + 1), a
	ld	a, 0
	sbc	a, (ix + 2)
	ld	(ix + 2), a
	ld	a, 0
	sbc	a, (ix + 3)
	ld	(ix + 3), a
	ret

; iy -> the dividend, which becomes the quotient; de -> the divisor, which is
; overwritten by the remainder. Both unsigned. Every register but af comes
; back as it went in.
;
; Shift and subtract, a bit of quotient a turn, with all four numbers in
; registers -- the quotient in b:ix, the remainder in c:hl, the divisor in
; iyl:de and the count in iyh -- where it used to keep them in memory and
; step each a byte at a time: 5,700 cycles a division, which was three
; quarters of test/perf's crc.c, whose Adler-32 takes two remainders a byte.
; The alternate registers are left alone: MOS's use of them is its own.
;
; The quotient takes the dividend's place a bit at a time: each turn shifts
; the dividend's top bit out into the remainder and leaves a zero at the
; bottom, which is the quotient's bit when the divisor does not fit. The
; remainder never shifts a bit out of its top: before the kth turn it is
; less than the dividend's top k-1 bits, so below 2^31 even before the
; last.
.ludivmod_core:
	push	bc
	push	de
	push	hl
	push	ix
	push	iy
	push	de			; where the remainder goes
	push	iy			; and the quotient

	; The dividend: b:ix, from its first byte that is not zero -- the
	; turns for the bytes above it would shift zeros through a zero
	; remainder and give zeros -- with c the turns that are left. A
	; dividend under 2^24 takes 24 turns, under 2^16 16: Adler-32's are
	; under 2^17. The bytes below the first one read are read from
	; before the dividend and cleared.
	ld	c, 32
	ld	a, (iy + 3)
	or	a, a
	jr	nz, .ldiv_32
	ld	c, 24
	ld	a, (iy + 2)
	or	a, a
	jr	nz, .ldiv_24
	ld	c, 16
	ld	a, (iy + 1)
	or	a, a
	jr	nz, .ldiv_16
	ld	c, 8
	ld	b, (iy + 0)
	ld	ix, 0
	jr	.ldiv_divisor
.ldiv_16:
	ld	b, a
	ld	ix, (iy - 2)
	ld	ixl, 0
	ld	ixh, 0
	jr	.ldiv_divisor
.ldiv_24:
	ld	b, a
	ld	ix, (iy - 1)
	ld	ixl, 0
	jr	.ldiv_divisor
.ldiv_32:
	ld	b, a
	ld	ix, (iy + 0)
.ldiv_divisor:
	ex	de, hl			; the divisor: iyl:de
	ld	de, (hl)
	inc	hl
	inc	hl
	inc	hl
	ld	a, (hl)
	ld	iyl, a
	ld	a, c
	ld	iyh, a			; the count

	ld	a, iyl			; a zero divisor is undefined in C:
	or	a, a			; say zero for both rather than loop
	jr	nz, .ldiv_go
	sbc	hl, hl
	sbc	hl, de
	jr	nz, .ldiv_go
	ld	ix, 0
	ld	b, 0
	ld	c, 0
	jr	.ldiv_store

.ldiv_go:
	or	a, a
	sbc	hl, hl			; the remainder: c:hl = 0
	ld	c, l

.ldiv_loop:
	add	ix, ix			; {remainder:quotient} <<= 1
	rl	b
	adc	hl, hl
	rl	c			; which leaves carry clear
	sbc	hl, de
	ld	a, c
	sbc	a, iyl
	jr	c, .ldiv_back
	ld	c, a
	inc	ix			; the quotient's bit
	dec	iyh
	jr	nz, .ldiv_loop
	jr	.ldiv_store
.ldiv_back:
	add	hl, de			; c was never written
	dec	iyh
	jr	nz, .ldiv_loop

.ldiv_store:
	pop	iy			; the quotient where the dividend was
	ld	(iy + 0), ix
	ld	(iy + 3), b
	pop	iy			; the remainder where the divisor was
	ld	(iy + 0), hl
	ld	(iy + 3), c
	pop	iy
	pop	ix
	pop	hl
	pop	de
	pop	bc
	ret

_acc_rt_ldivu:
	push	iy
	push	hl
	pop	iy
	call	.ludivmod_core
	pop	iy
	ret

_acc_rt_lremu:
	push	ix
	push	iy
	push	hl
	pop	iy
	push	de
	pop	ix
	call	.ludivmod_core
	ld	a, (ix + 0)		; the remainder is the answer, and the
	ld	(iy + 0), a		; core left it where the divisor was
	ld	a, (ix + 1)
	ld	(iy + 1), a
	ld	a, (ix + 2)
	ld	(iy + 2), a
	ld	a, (ix + 3)
	ld	(iy + 3), a
	pop	iy
	pop	ix
	ret

; C99 has division truncate towards zero and the remainder take the sign of
; the dividend, which is what dividing the magnitudes and fixing the sign
; afterwards gives.
_acc_rt_ldivs:
	push	ix
	push	iy
	push	bc
	push	hl
	pop	iy
	push	de
	pop	ix
	ld	a, (iy + 3)		; the quotient is negative when the
	xor	a, (ix + 3)		; operands differ in sign
	and	a, 0x80
	ld	b, a
	call	.labs_iy
	call	.labs_ix
	call	.ludivmod_core
	bit	7, b
	call	nz, .lneg_iy
	pop	bc
	pop	iy
	pop	ix
	ret

_acc_rt_lrems:
	push	ix
	push	iy
	push	bc
	push	hl
	pop	iy
	push	de
	pop	ix
	ld	a, (iy + 3)		; the remainder takes the dividend's
	and	a, 0x80			; sign
	ld	b, a
	call	.labs_iy
	call	.labs_ix
	call	.ludivmod_core
	ld	a, (ix + 0)
	ld	(iy + 0), a
	ld	a, (ix + 1)
	ld	(iy + 1), a
	ld	a, (ix + 2)
	ld	(iy + 2), a
	ld	a, (ix + 3)
	ld	(iy + 3), a
	bit	7, b
	call	nz, .lneg_iy
	pop	bc
	pop	iy
	pop	ix
	ret

; --------------------------------------------------- long unary
; (hl) = -(hl) and (hl) = ~(hl), four bytes.
;
; The 24-bit forms are `0 - x` in HL and the same less one, which the chip can
; do in a register. A long cannot be held in one, so these work in the frame
; like the rest of the long routines.

	.global _acc_rt_lneg
	.global _acc_rt_lnot

_acc_rt_lneg:
	push	iy
	push	hl
	pop	iy
	call	.lneg_iy
	pop	iy
	ret

_acc_rt_lnot:
	push	iy
	push	hl
	pop	iy
	ld	a, (iy + 0)
	cpl
	ld	(iy + 0), a
	ld	a, (iy + 1)
	cpl
	ld	(iy + 1), a
	ld	a, (iy + 2)
	cpl
	ld	(iy + 2), a
	ld	a, (iy + 3)
	cpl
	ld	(iy + 3), a
	pop	iy
	ret

; --------------------------------------------------- float
; IEEE-754 single precision, which is what agondev's float and double both
; are: a sign bit, eight exponent bits biased by 127, and twenty-three
; mantissa bits below a leading 1 that is not stored. Little endian, so the
; four bytes in memory are
;
;   b0  mantissa 7..0
;   b1  mantissa 15..8
;   b2  exponent bit 0 in bit 7, mantissa 22..16 below it
;   b3  sign in bit 7, exponent 7..1 below it
;
; The same four bytes agondev uses, which is the point: the two compilers
; have to be able to call each other.
;
; All four kinds of value are here: normals, denormals, infinities and NaNs.
; Denormals fill the gap between the smallest normal and zero, so a subtraction
; of two nearby small numbers gives their difference rather than zero; an
; exponent past the top gives an infinity rather than the largest finite
; number; and an operation with no answer -- infinity minus infinity, zero
; times infinity, zero over zero -- gives a NaN rather than something that
; looks like a number.
;
; What is not here is the part of IEEE-754 that is about the machine rather
; than the numbers: there are no exception flags, no rounding modes other than
; to nearest with ties to even, and a signalling NaN is treated as a quiet
; one. C99 makes all of that optional, and <fenv.h> is where a program would
; ask for it.

	.global _acc_rt_itof
	.global _acc_rt_uitof
	.global _acc_rt_ftoi

; hl = the int, de -> the four bytes to write.
;
; Every int on this machine converts exactly. A float keeps twenty-four bits
; of significand counting the one it does not store, and an int here is
; twenty-four bits wide, so there is never anything to round away -- which is
; why this is not the routine a long goes through.
_acc_rt_itof:
	push	iy
	push	bc
	push	hl			; saved
	push	hl			; the working copy, whose bytes need
	ld	iy, 0			; addresses the upper one can be
	add	iy, sp			; reached through

	ld	b, 0			; b = the sign, already in place
	bit	7, (iy + 2)
	jr	z, .itof_magnitude
	ld	b, 0x80
	ld	a, 0
	sub	a, (iy + 0)
	ld	(iy + 0), a
	ld	a, 0			; ld leaves the borrow alone
	sbc	a, (iy + 1)
	ld	(iy + 1), a
	ld	a, 0
	sbc	a, (iy + 2)
	ld	(iy + 2), a
	jr	.itof_magnitude

_acc_rt_uitof:
	push	iy
	push	bc
	push	hl
	push	hl
	ld	iy, 0
	add	iy, sp
	ld	b, 0			; never negative, so no sign and no
					; magnitude to take

.itof_magnitude:
	ld	a, (iy + 0)		; zero is the one value with no leading
	or	a, (iy + 1)		; 1 to find
	or	a, (iy + 2)
	jr	nz, .itof_normalise

	xor	a, a
	ld	(de), a
	inc	de
	ld	(de), a
	inc	de
	ld	(de), a
	inc	de
	ld	(de), a
	dec	de
	dec	de
	dec	de
	jr	.itof_done

.itof_normalise:
	ld	c, 150			; 127 + 23: the exponent when the top
					; bit is already bit 23
.itof_shift:
	bit	7, (iy + 2)
	jr	nz, .itof_pack
	sla	(iy + 0)
	rl	(iy + 1)
	rl	(iy + 2)
	dec	c
	jr	.itof_shift

.itof_pack:
	res	7, (iy + 2)		; the leading 1 is implied, not stored
	bit	0, c			; and the exponent's low bit takes the
	jr	z, .itof_exp_even	; place it leaves
	set	7, (iy + 2)
.itof_exp_even:
	ld	a, c
	srl	a
	or	a, b			; the sign goes above the exponent
	ld	c, a

	ld	a, (iy + 0)
	ld	(de), a
	inc	de
	ld	a, (iy + 1)
	ld	(de), a
	inc	de
	ld	a, (iy + 2)
	ld	(de), a
	inc	de
	ld	a, c
	ld	(de), a
	dec	de
	dec	de
	dec	de

.itof_done:
	pop	bc			; the working copy, discarded
	pop	hl
	pop	bc
	pop	iy
	ret

; hl -> the float; the int comes back in hl, truncated towards zero as C says.
; A value too large for an int is undefined in C and is left to wrap.
_acc_rt_ftoi:
	push	iy
	push	bc
	push	de
	push	hl
	pop	iy			; iy -> the float

	ld	a, (iy + 3)		; the exponent, which is split across
	and	a, 0x7f			; two bytes with the sign above it
	add	a, a
	ld	b, a
	ld	a, (iy + 2)
	rlca
	and	a, 1
	add	a, b
	ld	b, a			; b = the biased exponent

	ld	c, 0			; c = the sign
	bit	7, (iy + 3)
	jr	z, .ftoi_significand
	ld	c, 1

.ftoi_significand:
	ld	e, (iy + 0)		; the stored mantissa with its leading
	ld	d, (iy + 1)		; 1 put back
	ld	a, (iy + 2)
	and	a, 0x7f
	or	a, 0x80

	ld	hl, -3			; somewhere the three bytes can be
	add	hl, sp			; shifted as one value
	ld	sp, hl
	push	hl
	pop	iy
	ld	(iy + 0), e
	ld	(iy + 1), d
	ld	(iy + 2), a

	ld	a, b
	cp	a, 127			; below 1, so the integer part is 0
	jr	c, .ftoi_zero
	sub	a, 127			; how far the point has moved right
	ld	b, a
	ld	a, 23
	sub	a, b
	jr	c, .ftoi_left		; past the top of the significand

	or	a, a			; already an integer when a is zero
	jr	z, .ftoi_signed
	ld	b, a
.ftoi_right:
	srl	(iy + 2)
	rr	(iy + 1)
	rr	(iy + 0)
	djnz	.ftoi_right
	jr	.ftoi_signed

.ftoi_left:
	neg				; a was 23 - shift and went negative
	ld	b, a
.ftoi_left_loop:
	sla	(iy + 0)
	rl	(iy + 1)
	rl	(iy + 2)
	djnz	.ftoi_left_loop
	jr	.ftoi_signed

.ftoi_zero:
	ld	(iy + 0), 0
	ld	(iy + 1), 0
	ld	(iy + 2), 0

.ftoi_signed:
	bit	0, c
	jr	z, .ftoi_out
	ld	a, 0
	sub	a, (iy + 0)
	ld	(iy + 0), a
	ld	a, 0
	sbc	a, (iy + 1)
	ld	(iy + 1), a
	ld	a, 0
	sbc	a, (iy + 2)
	ld	(iy + 2), a

.ftoi_out:
	ld	hl, (iy + 0)
	ld	iy, 3			; give the three bytes back
	add	iy, sp
	ld	sp, iy
	pop	de
	pop	bc
	pop	iy
	ret

; The four bytes at (hl) rewritten as an unsigned key that sorts the same way
; the float does. Comparing floats is then the long comparison already here,
; rather than a second four-byte compare that knows about exponents.
;
; IEEE-754 was laid out so that the bits of two floats of the same sign
; already compare as integers -- the exponent sits above the mantissa for
; exactly that reason. Only the sign spoils it, and only in two ways: a
; negative float has its top bit set where a positive one does not, and among
; negatives the order runs backwards. Setting the top bit of a positive and
; complementing the whole of a negative fixes both.
;
; Zero is the case that needs saying out loud. -0.0 and 0.0 are different
; bytes and the same number, so -0.0 is turned into 0.0 before the transform
; and the two come out with one key. Without that they would be different
; keys, and `-0.0 < 0.0` would be true.

	.global _acc_rt_fkey
	.global _acc_rt_fcmp

; Z clear if the float at (hl) is a NaN: every exponent bit set, which an
; infinity also has, and a mantissa that is not empty, which only a NaN has.
.fisnan:
	push	iy
	push	hl
	pop	iy
	ld	a, (iy + 3)
	and	a, 0x7f
	cp	a, 0x7f
	jr	nz, .fisnan_no
	bit	7, (iy + 2)		; the exponent's low bit
	jr	z, .fisnan_no
	ld	a, (iy + 2)		; the mantissa, which an infinity leaves
	and	a, 0x7f			; empty
	or	a, (iy + 1)
	or	a, (iy + 0)
	pop	iy
	ret

.fisnan_no:
	xor	a, a
	pop	iy
	ret

; How the float at (hl) stands to the one at (de), in a: 0 below, 1 equal, 2
; above, 3 unordered. Both are rewritten as keys on the way, which the caller
; can afford because it passes copies.
;
; Three outcomes would do for integers; floats need a fourth. A NaN is not
; less than, equal to or greater than anything, itself included, so `x < y`
; and `x >= y` are both false when either is one -- which no ordering can
; express, and which is why this returns a code rather than leaving flags for
; the caller's branch to read.
_acc_rt_fcmp:
	push	ix
	push	iy
	push	de
	push	hl

	call	.fisnan
	jr	nz, .fcmp_unordered
	ex	de, hl
	call	.fisnan
	ex	de, hl
	jr	nz, .fcmp_unordered

	call	_acc_rt_fkey
	ex	de, hl
	call	_acc_rt_fkey
	ex	de, hl

	push	hl
	pop	iy
	push	de
	pop	ix
	ld	a, (iy + 3)
	cp	a, (ix + 3)
	jr	nz, .fcmp_differ
	ld	a, (iy + 2)
	cp	a, (ix + 2)
	jr	nz, .fcmp_differ
	ld	a, (iy + 1)
	cp	a, (ix + 1)
	jr	nz, .fcmp_differ
	ld	a, (iy + 0)
	cp	a, (ix + 0)
	jr	nz, .fcmp_differ
	ld	a, 1
	jr	.fcmp_done

.fcmp_differ:
	jr	c, .fcmp_below
	ld	a, 2
	jr	.fcmp_done
.fcmp_below:
	xor	a, a
	jr	.fcmp_done
.fcmp_unordered:
	ld	a, 3

.fcmp_done:
	pop	hl
	pop	de
	pop	iy
	pop	ix
	ret

_acc_rt_fkey:
	push	iy
	push	bc
	push	hl
	pop	iy

	ld	a, (iy + 0)		; zero, whatever its sign, becomes +0
	or	a, (iy + 1)
	or	a, (iy + 2)
	ld	b, a
	ld	a, (iy + 3)
	and	a, 0x7f
	or	a, b
	jr	nz, .fkey_signed
	ld	(iy + 3), 0

.fkey_signed:
	bit	7, (iy + 3)
	jr	nz, .fkey_negative
	set	7, (iy + 3)		; positive: above every negative
	jr	.fkey_done

.fkey_negative:
	ld	a, (iy + 0)		; negative: below every positive, and
	cpl				; in the opposite order to its bits
	ld	(iy + 0), a
	ld	a, (iy + 1)
	cpl
	ld	(iy + 1), a
	ld	a, (iy + 2)
	cpl
	ld	(iy + 2), a
	ld	a, (iy + 3)
	cpl
	ld	(iy + 3), a

.fkey_done:
	pop	bc
	pop	iy
	ret

; --------------------------------------------------- float add and subtract
; (hl) = (hl) + (de), and the same less the sign of the right operand.
;
; Both significands are unpacked into a thirty-two bit field with the
; twenty-four bits of the number at the top and eight spare bits below them.
; Those eight are what makes the result the one IEEE asks for rather than
; merely close: aligning the smaller operand shifts bits down into them, and
; the bit that falls off the bottom is kept by OR-ing it back into the lowest
; one, so that a value that is not exactly half way up cannot pretend to be.
; Rounding then reads that byte -- above half rounds up, below half truncates,
; and exactly half goes to the even significand, which is the tie rule that
; keeps a long run of sums from drifting.
;
; The frame, reached through ix:
;   ix+0..3   the left significand, the number in bits 31..8
;   ix+4..7   the right, the same way
;   ix+8      the left exponent          ix+9   the right
;   ix+10     the left sign in bit 7     ix+11  the right

	.global _acc_rt_fadd
	.global _acc_rt_fsub

; Unpack the float at (hl) into the four bytes at (iy), the exponent into b and
; the sign into c. An exponent of zero is a zero or a denormal, and both come
; out as zero: there are no denormals here.
; The significand at iy shifted up until its leading bit is at the top of
; its four bytes, and in A how many places that took: none for a normal
; number, some for a denormal. Only for one that is not zero.
.fnorm:
	xor	a, a
.fnorm_loop:
	bit	7, (iy + 3)
	ret	nz
	sla	(iy + 0)
	rl	(iy + 1)
	rl	(iy + 2)
	rl	(iy + 3)
	inc	a
	jr	.fnorm_loop

.funpack:
	ld	a, (hl)
	ld	(iy + 1), a		; mantissa 7..0
	inc	hl
	ld	a, (hl)
	ld	(iy + 2), a		; mantissa 15..8
	inc	hl
	ld	a, (hl)
	and	a, 0x7f
	or	a, 0x80			; the leading 1, which is not stored
	ld	(iy + 3), a
	ld	(iy + 0), 0		; the eight spare bits below the number
	ld	a, (hl)
	rlca				; the exponent's low bit
	and	a, 1
	ld	b, a
	inc	hl
	ld	a, (hl)
	ld	c, a
	and	a, 0x7f
	add	a, a
	add	a, b
	ld	b, a			; b = the biased exponent
	ld	a, c
	and	a, 0x80
	ld	c, a			; c = the sign, in place
	dec	hl
	dec	hl
	dec	hl

	; What the stored exponent means at its two ends.
	;
	; Zero means there is no leading 1 to put back, so the one written
	; above has to come off again. If what is left is nothing the number
	; is zero; if it is not, the number is a denormal -- a value too small
	; to have a leading 1 of its own, filling in the gap between the
	; smallest normal float and zero. A denormal is 0.mantissa x 2^-126,
	; and a normal at exponent 1 is 1.mantissa x 2^-126, so the two are on
	; the same scale: calling the denormal's exponent 1 and leaving its
	; leading bit clear makes every routine after this treat it like any
	; other number, with no denormal case in the arithmetic at all.
	;
	; 255 means an infinity when the mantissa is empty and a NaN when it
	; is not. The leading 1 comes off there too, so that the caller can
	; tell the two apart by whether the significand is zero.
	ld	a, b
	cp	a, 255
	jr	z, .funpack_special
	or	a, a
	ret	nz

	res	7, (iy + 3)		; no leading 1 after all
	ld	a, (iy + 1)
	or	a, (iy + 2)
	or	a, (iy + 3)
	ret	z			; nothing left: the number is zero
	inc	b			; a denormal, on the same scale as a
	ret				; normal at exponent 1

.funpack_special:
	res	7, (iy + 3)		; zero significand means an infinity,
	ret				; anything else a NaN

; The right operand is written to, which is why helper_writes_right in gen.c
; names this routine: acc passes a right operand where the program keeps it
; unless the routine is one of the few that change it.
_acc_rt_fsub:
	push	hl			; the same as adding the right operand
	push	de			; with its sign turned over, and the
	push	bc			; operand is the caller's scratch slot,
	ex	de, hl			; dead once the operator has been applied
	ld	bc, 3
	add	hl, bc
	ld	a, (hl)
	xor	a, 0x80
	ld	(hl), a
	pop	bc
	pop	de
	pop	hl
	; fall through

_acc_rt_fadd:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl

	ld	ix, -24			; the same size fmul uses, because the
	add	ix, sp			; two share everything from the
	ld	sp, ix			; rounding onwards
	ld	(ix + 12), hl		; the destination, which is also the left
					; operand, kept where clobbering hl
					; cannot lose it

	push	ix
	pop	iy
	call	.funpack
	ld	(ix + 8), b
	ld	(ix + 10), c

	push	ix
	pop	iy
	ld	bc, 4
	add	iy, bc
	ex	de, hl			; the right operand's address
	call	.funpack
	ex	de, hl
	ld	(ix + 9), b
	ld	(ix + 11), c

	; An infinity or a NaN on either side is answered here rather than put
	; through the arithmetic, which has no meaning for either. funpack
	; leaves both with an exponent of 255 and tells them apart by the
	; significand: empty is an infinity, anything else a NaN.
	ld	a, (ix + 8)
	cp	a, 255
	jp	z, .fadd_left_special
	ld	a, (ix + 9)
	cp	a, 255
	jp	z, .fadd_right_special

	; The larger exponent has to be the left one, so that aligning only
	; ever shifts the right operand down.
	ld	a, (ix + 8)
	cp	a, (ix + 9)
	jr	nc, .fadd_aligned_order
	call	.fadd_swap
.fadd_aligned_order:
	ld	a, (ix + 8)
	cp	a, (ix + 9)
	jr	nz, .fadd_align
	; Equal exponents: the larger significand has to be on the left too,
	; because a subtract here must not borrow past the top.
	ld	a, (ix + 3)
	cp	a, (ix + 7)
	jr	c, .fadd_need_swap
	jr	nz, .fadd_align
	ld	a, (ix + 2)
	cp	a, (ix + 6)
	jr	c, .fadd_need_swap
	jr	nz, .fadd_align
	ld	a, (ix + 1)
	cp	a, (ix + 5)
	jr	c, .fadd_need_swap
	jr	.fadd_align
.fadd_need_swap:
	call	.fadd_swap

.fadd_align:
	ld	a, (ix + 8)		; how far the right operand is down
	sub	a, (ix + 9)
	jr	z, .fadd_combine
	cp	a, 33			; further than the field is wide: all
	jr	c, .fadd_shift_loop	; that is left of it is a sticky bit
	ld	a, (ix + 4)
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	ld	(ix + 4), 0
	ld	(ix + 5), 0
	ld	(ix + 6), 0
	ld	(ix + 7), 0
	jr	z, .fadd_combine
	ld	(ix + 4), 1
	jp	.fadd_combine

.fadd_shift_loop:
	ld	b, a
.fadd_shift:
	srl	(ix + 7)
	rr	(ix + 6)
	rr	(ix + 5)
	rr	(ix + 4)
	jr	nc, .fadd_shift_next
	set	0, (ix + 4)		; what falls off the bottom is not lost,
					; it is remembered in the lowest bit
.fadd_shift_next:
	djnz	.fadd_shift

.fadd_combine:
	ld	a, (ix + 10)		; like signs add, unlike signs subtract
	xor	a, (ix + 11)
	jp	m, .fadd_subtract

	ld	a, (ix + 0)
	add	a, (ix + 4)
	ld	(ix + 0), a
	ld	a, (ix + 1)
	adc	a, (ix + 5)
	ld	(ix + 1), a
	ld	a, (ix + 2)
	adc	a, (ix + 6)
	ld	(ix + 2), a
	ld	a, (ix + 3)
	adc	a, (ix + 7)
	ld	(ix + 3), a
	jr	nc, .fadd_normalise
	; carried out of the top: one place right, and one more exponent
	rr	(ix + 3)		; the carry is the bit coming back in
	rr	(ix + 2)
	rr	(ix + 1)
	rr	(ix + 0)
	jr	nc, .fadd_carry_exp
	set	0, (ix + 0)
.fadd_carry_exp:
	inc	(ix + 8)
	jp	z, .fadd_infinity
	jp	.fadd_round

.fadd_subtract:
	ld	a, (ix + 0)
	sub	a, (ix + 4)
	ld	(ix + 0), a
	ld	a, (ix + 1)
	sbc	a, (ix + 5)
	ld	(ix + 1), a
	ld	a, (ix + 2)
	sbc	a, (ix + 6)
	ld	(ix + 2), a
	ld	a, (ix + 3)
	sbc	a, (ix + 7)
	ld	(ix + 3), a

.fadd_normalise:
	ld	a, (ix + 0)		; an exact cancellation is +0, which is
	or	a, (ix + 1)		; what IEEE asks for in this rounding
	or	a, (ix + 2)		; mode, whatever the operands' signs
	or	a, (ix + 3)
	jr	nz, .fadd_norm_loop
	ld	(ix + 10), 0
	jp	.fadd_zero
.fadd_norm_loop:
	bit	7, (ix + 3)
	jr	nz, .fadd_round
	ld	a, (ix + 8)		; exponent 1 is as low as a number goes.
	cp	a, 2			; What is left without a leading bit is
	jr	c, .fadd_round		; a denormal, and packs as one.
	sla	(ix + 0)
	rl	(ix + 1)
	rl	(ix + 2)
	rl	(ix + 3)
	dec	(ix + 8)
	jr	.fadd_norm_loop

; The exponent in hl is at or below zero, so the result is too small to have
; a leading bit of its own. Shifting the significand down by as much as the
; exponent is short brings it to the denormal scale, where the exponent is 1
; and the leading bit is wherever it falls. The bits pushed out are kept in
; the lowest one, so that rounding still sees the difference between a value
; on the halfway mark and one just above it.
.fadd_subnormal:
	ld	de, 1
	ex	de, hl
	or	a, a
	sbc	hl, de			; hl = 1 - the exponent
	ld	de, 26
	or	a, a
	sbc	hl, de
	jp	nc, .fadd_zero		; further down than the field is wide
	add	hl, de
	push	hl
	pop	de
	ld	b, e
	ld	(ix + 8), 1
.fadd_sub_loop:
	srl	(ix + 3)
	rr	(ix + 2)
	rr	(ix + 1)
	rr	(ix + 0)
	jr	nc, .fadd_sub_next
	set	0, (ix + 0)
.fadd_sub_next:
	djnz	.fadd_sub_loop
	; fall through to rounding

.fadd_round:
	ld	a, (ix + 0)		; the eight bits below the number
	cp	a, 0x80
	jr	c, .fadd_pack		; below half: truncate
	jr	nz, .fadd_round_up	; above half: up
	bit	0, (ix + 1)		; exactly half: to the even significand
	jr	z, .fadd_pack
.fadd_round_up:
	ld	a, (ix + 1)
	add	a, 1
	ld	(ix + 1), a
	ld	a, (ix + 2)
	adc	a, 0
	ld	(ix + 2), a
	ld	a, (ix + 3)
	adc	a, 0
	ld	(ix + 3), a
	jr	nc, .fadd_pack
	; rounding carried out of the top, so the number is a power of two
	ld	(ix + 3), 0x80
	inc	(ix + 8)
	jp	z, .fadd_infinity

.fadd_pack:
	ld	a, (ix + 8)
	cp	a, 255
	jp	nc, .fadd_infinity

	; Which exponent gets written is what says normal from denormal, and
	; the significand's leading bit is what decides. A normal one has it
	; set and it is implied rather than stored; a denormal has it clear
	; and a stored exponent of zero says so. Rounding a denormal up can
	; set it, which turns the result into the smallest normal -- and that
	; needs no special case here, because by then the bit is set and the
	; exponent to write is the 1 it already has.
	bit	7, (ix + 3)
	jr	nz, .fadd_pack_write
	ld	(ix + 8), 0

.fadd_pack_write:
	ld	hl, (ix + 12)
	ld	a, (ix + 1)
	ld	(hl), a
	inc	hl
	ld	a, (ix + 2)
	ld	(hl), a
	inc	hl
	ld	a, (ix + 3)
	and	a, 0x7f			; the leading 1 goes back to being implied
	ld	b, a
	ld	a, (ix + 8)
	rrca				; the exponent's low bit sits above the
	and	a, 0x80			; mantissa
	or	a, b
	ld	(hl), a
	inc	hl
	ld	a, (ix + 8)
	srl	a
	or	a, (ix + 10)		; and the sign above the exponent
	ld	(hl), a
	jp	.fadd_done

; An exponent past the top. IEEE says the answer is an infinity of the right
; sign, and a program that goes on to compute with one gets an infinity or a
; NaN out rather than a large number that looks like an ordinary answer.
.fadd_infinity:
	ld	hl, (ix + 12)
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	a, (ix + 10)
	or	a, 0x7f
	ld	(hl), a
	dec	hl
	ld	(hl), 0x80		; the exponent's low bit, which lives
	jp	.fadd_done		; above the mantissa

; A quiet NaN, which is what an operation with no answer produces: every
; exponent bit set, as an infinity has, and a mantissa that is not empty. The
; top mantissa bit is the one that makes it quiet rather than signalling.
.fadd_nan:
	ld	hl, (ix + 12)
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	(hl), 0xc0
	inc	hl
	ld	(hl), 0x7f
	jp	.fadd_done

; Zero takes the sign it was given. An exact cancellation clears that sign
; first, because IEEE makes x - x a positive zero in this rounding mode
; whatever the operands were.
.fadd_zero:
	ld	hl, (ix + 12)
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	a, (ix + 10)
	ld	(hl), a

.fadd_done:
	ld	hl, 24
	add	hl, sp
	ld	sp, hl
	pop	hl
	pop	de
	pop	bc
	pop	iy
	pop	ix
	ret

.fadd_left_special:
	ld	a, (ix + 0)
	or	a, (ix + 1)
	or	a, (ix + 2)
	or	a, (ix + 3)
	jp	nz, .fadd_nan		; a NaN takes everything with it

	ld	a, (ix + 9)		; an infinity, and the right side finite:
	cp	a, 255			; no finite number moves an infinity
	jp	nz, .fadd_infinity

	ld	a, (ix + 4)
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	jp	nz, .fadd_nan		; a NaN on the right

	ld	a, (ix + 10)		; two infinities: the same sign gives
	xor	a, (ix + 11)		; that infinity back, and opposite signs
	jp	nz, .fadd_nan		; have no answer at all
	jp	.fadd_infinity

.fadd_right_special:
	ld	a, (ix + 4)
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	jp	nz, .fadd_nan

	ld	a, (ix + 11)		; the sum is the right side's infinity,
	ld	(ix + 10), a		; sign and all
	jp	.fadd_infinity

; The two operands exchanged: the significands, which are four bytes four
; apart, and then the exponents and the signs, which are one byte one apart.
; Walking six pairs four apart instead was the same loop written once for
; three groups that are not laid out alike, and it swapped the right
; significand with the left exponent.
.fadd_swap:
	push	bc
	push	iy
	push	ix
	pop	iy
	ld	b, 4
.fadd_swap_sig:
	ld	a, (iy + 0)
	ld	c, a
	ld	a, (iy + 4)
	ld	(iy + 0), a
	ld	a, c
	ld	(iy + 4), a
	inc	iy
	djnz	.fadd_swap_sig

	ld	a, (ix + 8)
	ld	c, a
	ld	a, (ix + 9)
	ld	(ix + 8), a
	ld	a, c
	ld	(ix + 9), a

	ld	a, (ix + 10)
	ld	c, a
	ld	a, (ix + 11)
	ld	(ix + 10), a
	ld	a, c
	ld	(ix + 11), a
	pop	iy
	pop	bc
	ret

; --------------------------------------------------- float multiply
; (hl) = (hl) * (de).
;
; The significands are twenty-four bits each and their product is forty-eight,
; built from the nine partial products of their bytes -- MLT again, and this
; time none of the nine can be dropped, because the top of the product is
; exactly the part that is kept.
;
; Two twenty-four bit numbers with their leading bit set multiply to something
; in [2^46, 2^48), so the answer needs at most one shift to put its leading
; bit at 47. Which of the two cases it is decides the exponent: a product that
; already reaches bit 47 is between 2 and 4 and takes one more exponent than
; the sum of the operands'.
;
; The frame is laid out like fadd's on purpose -- significand at ix+1..3,
; exponent at ix+8, sign at ix+10, destination at ix+12 -- so that the
; rounding and packing at the end of fadd serve this too. What is above ix+15
; is the part fadd has no use for: the forty-eight bit product, and the
; exponent before it is known to fit in a byte.

	.global _acc_rt_fmul

; The 16-bit product of significand byte \i of the left and \j of the right,
; added into the running product at byte \k and carried up to the top.
	.macro	FMUL_AT i, j, k
	ld	b, (ix + 1 + \i)
	ld	c, (ix + 5 + \j)
	mlt	bc
	ld	a, (ix + 16 + \k)
	add	a, c
	ld	(ix + 16 + \k), a
	ld	a, (ix + 17 + \k)
	adc	a, b
	ld	(ix + 17 + \k), a
	.if \k < 4
	ld	a, (ix + 18 + \k)
	adc	a, 0
	ld	(ix + 18 + \k), a
	.endif
	.if \k < 3
	ld	a, (ix + 19 + \k)
	adc	a, 0
	ld	(ix + 19 + \k), a
	.endif
	.if \k < 2
	ld	a, (ix + 20 + \k)
	adc	a, 0
	ld	(ix + 20 + \k), a
	.endif
	.if \k < 1
	ld	a, (ix + 21 + \k)
	adc	a, 0
	ld	(ix + 21 + \k), a
	.endif
	.endm

_acc_rt_fmul:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl

	ld	ix, -24
	add	ix, sp
	ld	sp, ix
	ld	(ix + 12), hl

	push	ix
	pop	iy
	call	.funpack
	ld	(ix + 8), b
	ld	(ix + 10), c

	push	ix
	pop	iy
	ld	bc, 4
	add	iy, bc
	ex	de, hl
	call	.funpack
	ex	de, hl
	ld	(ix + 9), b
	ld	(ix + 11), c

	ld	a, (ix + 10)		; the sign is the two signs differing
	xor	a, (ix + 11)
	and	a, 0x80
	ld	(ix + 10), a

	ld	a, (ix + 8)		; an infinity or a NaN, as in fadd
	cp	a, 255
	jp	z, .fmul_left_special
	ld	a, (ix + 9)
	cp	a, 255
	jp	z, .fmul_right_special

	ld	a, (ix + 8)		; either operand zero makes the product
	or	a, a			; zero, which keeps the sign the two
	jp	z, .fadd_zero		; operands gave it
	ld	a, (ix + 9)
	or	a, a
	jp	z, .fadd_zero

	; A denormal comes out of .funpack with its leading bit clear, and the
	; product below expects both leading bits at the top: shifted up here,
	; with the places it took coming off the exponent.
	push	ix
	pop	iy
	call	.fnorm
	ld	(ix + 22), a
	lea	iy, ix + 4
	call	.fnorm
	add	a, (ix + 22)
	ld	(ix + 22), a

	ld	(ix + 16), 0		; the product, in six bytes
	ld	(ix + 17), 0
	ld	(ix + 18), 0
	ld	(ix + 19), 0
	ld	(ix + 20), 0
	ld	(ix + 21), 0

	FMUL_AT 0, 0, 0
	FMUL_AT 0, 1, 1
	FMUL_AT 1, 0, 1
	FMUL_AT 0, 2, 2
	FMUL_AT 1, 1, 2
	FMUL_AT 2, 0, 2
	FMUL_AT 1, 2, 3
	FMUL_AT 2, 1, 3
	FMUL_AT 2, 2, 4

	; The exponent, which does not fit in a byte until the range has been
	; checked: two biased exponents add to as much as 508.
	ld	hl, 0
	ld	l, (ix + 8)
	ld	de, 0
	ld	e, (ix + 9)
	add	hl, de
	ld	de, 127			; one bias too many, having added two
	or	a, a
	sbc	hl, de
	ld	e, (ix + 22)		; and the places a denormal was moved
	or	a, a
	sbc	hl, de

	bit	7, (ix + 21)		; already at bit 47: between 2 and 4,
	jr	z, .fmul_shift		; so one more exponent
	inc	hl
	jr	.fmul_exponent

.fmul_shift:
	sla	(ix + 16)		; below bit 47: one place up, and the
	rl	(ix + 17)		; exponent is the sum as it stands
	rl	(ix + 18)
	rl	(ix + 19)
	rl	(ix + 20)
	rl	(ix + 21)

.fmul_exponent:
	; The result goes into the shape everything else rounds and packs: the
	; significand is the top twenty-four bits of the product, and the guard
	; byte below it is the next eight, with the rest of the product folded
	; into its lowest bit so that nothing below the guard is lost.
	ld	a, (ix + 21)
	ld	(ix + 3), a
	ld	a, (ix + 20)
	ld	(ix + 2), a
	ld	a, (ix + 19)
	ld	(ix + 1), a
	ld	a, (ix + 18)
	ld	(ix + 0), a
	ld	a, (ix + 16)
	or	a, (ix + 17)
	jr	z, .fmul_exp_range
	set	0, (ix + 0)

.fmul_exp_range:
	push	hl			; the exponent, as a value that may have
	pop	de			; gone negative or past a byte
	ld	a, d
	or	a, a
	jr	nz, .fmul_wide_exponent
	ld	a, e
	or	a, a
	jp	z, .fadd_subnormal	; zero and below is a denormal or nothing
	cp	a, 255
	jp	nc, .fadd_infinity
	ld	(ix + 8), e
	jp	.fadd_round

.fmul_wide_exponent:
	cp	a, 0xff			; the high byte says which end it ran off
	jp	z, .fadd_subnormal
	jp	.fadd_infinity

.fmul_left_special:
	ld	a, (ix + 0)
	or	a, (ix + 1)
	or	a, (ix + 2)
	or	a, (ix + 3)
	jp	nz, .fadd_nan

	ld	a, (ix + 9)		; an infinity times zero is the one
	or	a, a			; product with no answer: the two pull
	jp	z, .fadd_nan		; in opposite directions
	cp	a, 255
	jp	nz, .fadd_infinity
	ld	a, (ix + 4)		; the right side special as well
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	jp	nz, .fadd_nan
	jp	.fadd_infinity		; two infinities multiply to one

.fmul_right_special:
	ld	a, (ix + 4)
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	jp	nz, .fadd_nan
	ld	a, (ix + 8)
	or	a, a
	jp	z, .fadd_nan		; zero times an infinity
	jp	.fadd_infinity

; --------------------------------------------------- float divide
; (hl) = (hl) / (de).
;
; Both significands are twenty-four bits with the leading one set, so their
; ratio is between a half and two and the quotient needs at most one place of
; shifting -- which is decided up front by comparing them, and shows up as
; which of two biases the exponent takes.
;
; The division itself is the restoring loop the integers use, except that
; everything in it fits in a register: hl holds the remainder and de the
; divisor, both twenty-four bits, so a step is `add hl, hl` and one `sbc`.
; Doubling the remainder can carry out of twenty-four bits, and as in the long
; division that carry means the divisor fits whatever the borrow says.
;
; Twenty-four bits of quotient are produced, then one more as the guard bit,
; and whether anything is left over afterwards is the sticky: a remainder of
; zero with the guard set is a true tie and goes to the even significand,
; while any remainder at all makes it round up.
;
; The frame is fadd's, so the packing at the end of fadd finishes this too.

	.global _acc_rt_fdiv

_acc_rt_fdiv:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl

	ld	ix, -24
	add	ix, sp
	ld	sp, ix
	ld	(ix + 12), hl

	push	ix
	pop	iy
	call	.funpack
	ld	(ix + 8), b
	ld	(ix + 10), c

	push	ix
	pop	iy
	ld	bc, 4
	add	iy, bc
	ex	de, hl
	call	.funpack
	ex	de, hl
	ld	(ix + 9), b
	ld	(ix + 11), c

	ld	a, (ix + 10)		; the sign is the two signs differing
	xor	a, (ix + 11)
	and	a, 0x80
	ld	(ix + 10), a

	ld	a, (ix + 8)		; an infinity or a NaN, as in fadd
	cp	a, 255
	jp	z, .fdiv_left_special
	ld	a, (ix + 9)
	cp	a, 255
	jp	z, .fdiv_right_special

	ld	a, (ix + 8)
	or	a, a
	jr	nz, .fdiv_nonzero
	ld	a, (ix + 9)		; zero over zero has no answer; zero
	or	a, a			; over anything else is zero
	jp	z, .fadd_nan
	jp	.fadd_zero

.fdiv_nonzero:
	ld	a, (ix + 9)		; a finite number over zero is an
	or	a, a			; infinity, which is what IEEE says and
	jp	z, .fadd_infinity	; C leaves undefined

	jp	.fdiv_finite

.fdiv_left_special:
	ld	a, (ix + 0)
	or	a, (ix + 1)
	or	a, (ix + 2)
	or	a, (ix + 3)
	jp	nz, .fadd_nan

	ld	a, (ix + 9)		; an infinity over an infinity has no
	cp	a, 255			; answer; over anything else it is an
	jp	nz, .fadd_infinity	; infinity
	ld	a, (ix + 4)
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	jp	nz, .fadd_nan
	jp	.fadd_nan

.fdiv_right_special:
	ld	a, (ix + 4)
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	jp	nz, .fadd_nan
	jp	.fadd_zero		; a finite number over an infinity

.fdiv_finite:
	; Denormals shifted up to a leading bit, as in fmul: the dividend's
	; places come off the exponent and the divisor's go on.
	push	ix
	pop	iy
	call	.fnorm
	ld	(ix + 15), a
	lea	iy, ix + 4
	call	.fnorm
	ld	(ix + 23), a

	; Which bias the exponent takes, and how many bits the loop has to
	; produce: a dividend at least the divisor gives its leading 1 at once,
	; and one fewer iteration is needed for the same twenty-four bits.
	ld	de, (ix + 5)		; the divisor's significand
	ld	hl, (ix + 1)		; the dividend's
	or	a, a
	sbc	hl, de
	jr	nc, .fdiv_ge

	add	hl, de			; smaller: the subtract did not happen,
	ld	(ix + 16), hl		; so the whole dividend is the remainder
	ld	(ix + 0), 0		; and the whole quotient comes from the
	ld	(ix + 1), 0		; loop, one exponent lower
	ld	(ix + 2), 0
	ld	(ix + 3), 0
	ld	b, 32
	ld	c, 126
	jr	.fdiv_exponent

.fdiv_ge:
	ld	(ix + 16), hl		; what the leading 1 left behind
	ld	(ix + 0), 1		; and that 1, already in place
	ld	(ix + 1), 0
	ld	(ix + 2), 0
	ld	(ix + 3), 0
	ld	b, 31
	ld	c, 127

.fdiv_exponent:
	push	bc			; the count and the bias
	ld	hl, 0
	ld	l, (ix + 8)
	ld	de, 0
	ld	e, (ix + 9)
	or	a, a
	sbc	hl, de			; the exponents' difference, which may
	ld	de, 0			; have gone negative
	ld	e, c
	add	hl, de			; plus the bias
	ld	e, (ix + 23)		; plus the divisor's shift
	add	hl, de
	ld	e, (ix + 15)		; less the dividend's
	or	a, a
	sbc	hl, de

	ld	(ix + 20), hl		; kept until there is a quotient to go
	pop	bc			; with it, because whether it is in
					; range decides nothing until then

	ld	hl, (ix + 16)		; the remainder, in a register for the
	ld	de, (ix + 5)		; whole loop, as is the divisor

	; The quotient is built across the whole thirty-two bit field rather
	; than the twenty-four of the significand, so that its leading bit
	; lands at bit 31 and the eight below the number come out of the
	; division too. That is the shape everything else rounds and packs,
	; and it is why the count is 31 or 32 and not 24 or 25: the extra
	; iterations are the guard byte.
.fdiv_loop:
	sla	(ix + 0)		; the quotient makes room for the bit
	rl	(ix + 1)
	rl	(ix + 2)
	rl	(ix + 3)
	add	hl, hl			; and the remainder doubles
	jr	c, .fdiv_force
	or	a, a
	sbc	hl, de
	jr	nc, .fdiv_fits
	add	hl, de			; it did not fit, so put it back
	jr	.fdiv_next
.fdiv_force:
	or	a, a			; above twenty-four bits, so the divisor
	sbc	hl, de			; fits however the borrow reads
.fdiv_fits:
	inc	(ix + 0)		; the shift left bit 0 clear
.fdiv_next:
	djnz	.fdiv_loop

	; Whatever is left of the remainder goes into the lowest bit, so that a
	; quotient that stopped exactly on the halfway mark can be told from
	; one that merely rounds to it.
	ld	de, 0			; hl == 0 over all three bytes: `ld a, h`
	or	a, a			; would reach two of them and the third
	sbc	hl, de			; has no name, but a subtract of zero
	jr	z, .fdiv_rounded	; sets Z from the whole width
	set	0, (ix + 0)

.fdiv_rounded:
	ld	hl, (ix + 20)		; and now the exponent has to answer for
	push	hl			; itself
	pop	de
	ld	a, d
	or	a, a
	jr	nz, .fdiv_wide_exponent
	ld	a, e
	or	a, a
	jp	z, .fadd_subnormal
	cp	a, 255
	jp	nc, .fadd_infinity
	ld	(ix + 8), e
	jp	.fadd_round

.fdiv_wide_exponent:
	cp	a, 0xff			; the high byte says which end it ran off
	jp	z, .fadd_subnormal
	jp	.fadd_infinity

; --------------------------------------------------- long to float and back
; A long is thirty-two bits and a float keeps twenty-four of significand, so
; unlike an int a long does not always land on a float exactly. These are the
; routines that round it, which is why they are not itof with a wider input.
;
; Both work in place: the four bytes at (hl) are read as one type and written
; back as the other, which is what the caller wants -- a long and a float are
; the same width and live in the same kind of frame slot.
;
; ltof needs no rounding code of its own. Shifting the magnitude up until its
; leading bit reaches bit 31 leaves the number in bits 31..8 and whatever was
; below it in bits 7..0, which is exactly the shape fadd rounds and packs.

	.global _acc_rt_ltof
	.global _acc_rt_ultof
	.global _acc_rt_ftol

_acc_rt_ltof:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl

	ld	ix, -24
	add	ix, sp
	ld	sp, ix
	ld	(ix + 12), hl

	ld	b, 0			; the sign
	ld	a, (hl)
	ld	(ix + 0), a
	inc	hl
	ld	a, (hl)
	ld	(ix + 1), a
	inc	hl
	ld	a, (hl)
	ld	(ix + 2), a
	inc	hl
	ld	a, (hl)
	ld	(ix + 3), a

	bit	7, a
	jr	z, .ltof_magnitude
	ld	b, 0x80
	ld	a, 0
	sub	a, (ix + 0)
	ld	(ix + 0), a
	ld	a, 0			; ld leaves the borrow alone
	sbc	a, (ix + 1)
	ld	(ix + 1), a
	ld	a, 0
	sbc	a, (ix + 2)
	ld	(ix + 2), a
	ld	a, 0
	sbc	a, (ix + 3)
	ld	(ix + 3), a
	jr	.ltof_magnitude

_acc_rt_ultof:
.ultof_entry:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl

	ld	ix, -24
	add	ix, sp
	ld	sp, ix
	ld	(ix + 12), hl

	ld	b, 0			; never negative, so no magnitude to take
	ld	a, (hl)
	ld	(ix + 0), a
	inc	hl
	ld	a, (hl)
	ld	(ix + 1), a
	inc	hl
	ld	a, (hl)
	ld	(ix + 2), a
	inc	hl
	ld	a, (hl)
	ld	(ix + 3), a

.ltof_magnitude:
	ld	(ix + 10), b		; the sign, in the place pack reads it
	ld	a, (ix + 0)
	or	a, (ix + 1)
	or	a, (ix + 2)
	or	a, (ix + 3)
	jp	z, .fadd_zero

	ld	c, 158			; 127 + 31: the exponent when the top
					; bit is already bit 31
.ltof_shift:
	bit	7, (ix + 3)
	jr	nz, .ltof_done
	sla	(ix + 0)
	rl	(ix + 1)
	rl	(ix + 2)
	rl	(ix + 3)
	dec	c
	jr	.ltof_shift

.ltof_done:
	ld	(ix + 8), c
	jp	.fadd_round		; the eight bits below the number are
					; already where rounding looks for them

; hl -> the float, overwritten by the long. Truncated towards zero, as C says;
; a value too large for a long is undefined and is left to wrap.
_acc_rt_ftol:
.ftol_entry:				; for calls from inside the blob
	push	ix
	push	iy
	push	bc
	push	de
	push	hl

	ld	ix, -12
	add	ix, sp
	ld	sp, ix
	ld	(ix + 8), hl

	push	hl
	pop	iy

	ld	a, (iy + 3)		; the exponent, split across two bytes
	and	a, 0x7f
	add	a, a
	ld	b, a
	ld	a, (iy + 2)
	rlca
	and	a, 1
	add	a, b
	ld	b, a			; b = the biased exponent

	ld	c, 0			; c = the sign
	bit	7, (iy + 3)
	jr	z, .ftol_significand
	ld	c, 1

.ftol_significand:
	ld	(ix + 0), 0		; the significand in bits 31..8, which
	ld	a, (iy + 0)		; makes the value (ix+0..3) / 2^(158-e)
	ld	(ix + 1), a
	ld	a, (iy + 1)
	ld	(ix + 2), a
	ld	a, (iy + 2)
	and	a, 0x7f
	or	a, 0x80
	ld	(ix + 3), a

	ld	a, 158			; how far down the point has to come
	sub	a, b
	jr	c, .ftol_store		; past the top: undefined, so as it is
	cp	a, 32
	jr	nc, .ftol_zero		; everything shifts out
	or	a, a
	jr	z, .ftol_store
	ld	b, a
.ftol_shift:
	srl	(ix + 3)
	rr	(ix + 2)
	rr	(ix + 1)
	rr	(ix + 0)
	djnz	.ftol_shift
	jr	.ftol_store

.ftol_zero:
	ld	(ix + 0), 0
	ld	(ix + 1), 0
	ld	(ix + 2), 0
	ld	(ix + 3), 0

.ftol_store:
	bit	0, c
	jr	z, .ftol_out
	ld	a, 0
	sub	a, (ix + 0)
	ld	(ix + 0), a
	ld	a, 0
	sbc	a, (ix + 1)
	ld	(ix + 1), a
	ld	a, 0
	sbc	a, (ix + 2)
	ld	(ix + 2), a
	ld	a, 0
	sbc	a, (ix + 3)
	ld	(ix + 3), a

.ftol_out:
	ld	hl, (ix + 8)
	ld	a, (ix + 0)
	ld	(hl), a
	inc	hl
	ld	a, (ix + 1)
	ld	(hl), a
	inc	hl
	ld	a, (ix + 2)
	ld	(hl), a
	inc	hl
	ld	a, (ix + 3)
	ld	(hl), a

	ld	hl, 12
	add	hl, sp
	ld	sp, hl
	pop	hl
	pop	de
	pop	bc
	pop	iy
	pop	ix
	ret

; ================================================================ long long
; Everything from here on is emitted only into a program that uses a long
; long: acc_rt_split marks where the blob can be cut, and nothing above it
; calls anything below. What is below may call up.
;
; A long long is eight bytes and lives in the frame as a long does, so these
; take the same shape: HL points at the destination and left operand, DE at
; the right one, and everything but the destination is preserved. Where a
; long routine is a loop over its bytes, the long long one is the same loop
; entered with eight in B.

	.global	acc_rt_split
acc_rt_split:

	.global _acc_rt_lladd
	.global _acc_rt_llsub
	.global _acc_rt_lland
	.global _acc_rt_llor
	.global _acc_rt_llxor
	.global _acc_rt_llcmpeq
	.global _acc_rt_llcmpord
	.global _acc_rt_llneg
	.global _acc_rt_llnot
	.global _acc_rt_llshl
	.global _acc_rt_llshru
	.global _acc_rt_llshrs
	.global _acc_rt_llmul
	.global _acc_rt_lldivu
	.global _acc_rt_llremu
	.global _acc_rt_lldivs
	.global _acc_rt_llrems
	.global _acc_rt_lltof
	.global _acc_rt_ulltof
	.global _acc_rt_ftoll

_acc_rt_lladd:
	push	bc
	push	de
	push	hl
	ld	b, 8
	jp	.ladd_n

_acc_rt_llsub:
	push	bc
	push	de
	push	hl
	ld	b, 8
	jp	.lsub_n

_acc_rt_lland:
	push	bc
	push	de
	push	hl
	ld	b, 8
	jp	.land_n

_acc_rt_llor:
	push	bc
	push	de
	push	hl
	ld	b, 8
	jp	.lor_n

_acc_rt_llxor:
	push	bc
	push	de
	push	hl
	ld	b, 8
	jp	.lxor_n

_acc_rt_llcmpeq:
	push	bc
	push	de
	push	hl
	ld	b, 8
	jp	.lcmpeq_n

_acc_rt_llcmpord:
	push	bc
	push	de
	push	hl
	ld	b, 8
	jp	.lcmpord_n

; (hl) = -(hl): 0 minus each byte, the borrow chaining up. Neither `ld a, 0`
; nor `inc hl` nor djnz touches the carry.
_acc_rt_llneg:
.llneg_hl:
	push	bc
	push	hl
	ld	b, 8
	or	a, a
.llneg_loop:
	ld	a, 0
	sbc	a, (hl)
	ld	(hl), a
	inc	hl
	djnz	.llneg_loop
	pop	hl
	pop	bc
	ret

_acc_rt_llnot:
	push	bc
	push	hl
	ld	b, 8
.llnot_loop:
	ld	a, (hl)
	cpl
	ld	(hl), a
	inc	hl
	djnz	.llnot_loop
	pop	hl
	pop	bc
	ret

; (hl) shifted by the low byte of (de), masked to six bits. A bit at a time,
; each a pass over the eight bytes: rl and rr through (hl) carry the bit from
; one byte into the next, and the passes restart from the saved pointer.
_acc_rt_llshl:
	ld	a, (de)
	and	a, 63
	ret	z
	push	bc
	push	hl
	ld	c, a
.llshl_bit:
	pop	hl
	push	hl
	ld	b, 8
	or	a, a			; a zero comes in at the bottom
.llshl_byte:
	rl	(hl)
	inc	hl
	djnz	.llshl_byte
	dec	c
	jr	nz, .llshl_bit
	pop	hl
	pop	bc
	ret

; Right shifts walk down from the top byte. What comes in at the top is zero
; for an unsigned value and the sign for a signed one, and E says which.
_acc_rt_llshru:
	ld	a, (de)			; the count, before E is taken over
	push	de
	ld	e, 0
	jr	.llshr
_acc_rt_llshrs:
	ld	a, (de)
	push	de
	ld	e, 1
.llshr:
	and	a, 63
	jr	z, .llshr_none
	push	bc
	push	hl
	ld	bc, 7
	add	hl, bc			; hl -> the top byte
	ld	c, a
.llshr_bit:
	push	hl
	ld	b, 8
	or	a, a
	bit	0, e			; bit leaves the carry alone
	jr	z, .llshr_byte
	ld	a, (hl)
	rla				; the sign, into the carry
.llshr_byte:
	rr	(hl)
	dec	hl
	djnz	.llshr_byte
	pop	hl
	dec	c
	jr	nz, .llshr_bit
	pop	hl
	pop	bc
.llshr_none:
	pop	de
	ret

; (hl) = (hl) * (de), eight bytes, wrapping.
;
; Schoolbook, a byte at a time, as the long multiply is: mlt gives the 16-bit
; product of two bytes, and byte i of the left operand times byte j of the
; right one lands at byte i + j. Only the 36 products with i + j below eight
; land inside the answer, so row i -- byte i times the whole right operand,
; added in from byte i up -- is 8 - i steps long, and whatever it carries out
; of the top byte is dropped. A row whose byte is zero adds nothing and is
; skipped, which is most of them for a value that would have fitted a long.
;
; Each step adds a product, the byte of the answer already there and the
; carry from the step below, and the sum fits sixteen bits: 255 * 255 + 255 +
; 255 is 65535. Its low byte is the answer's and its high byte the carry.
;
; The left operand is copied out first, to ix+0..7, because the answer is
; built where it was.
_acc_rt_llmul:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl
	ld	ix, -8
	add	ix, sp
	ld	sp, ix

	push	hl
	pop	iy			; iy -> the answer
	push	de
	lea	de, ix + 0		; the left operand, copied
	ld	bc, 8
	ldir
	pop	de
	lea	hl, iy + 0		; and the answer, zero
	xor	a, a
	ld	b, 8
.llmul_clear:
	ld	(hl), a
	inc	hl
	djnz	.llmul_clear

	ld	c, 8			; row 0 is eight steps long
.llmul_row:
	ld	a, (ix + 0)		; this row's byte of the left operand
	or	a, a
	jr	z, .llmul_next
	push	bc
	push	de
	push	iy
	ld	h, c			; steps in this row
	ld	l, 0			; the carry into the first
.llmul_step:
	ld	a, (de)
	ld	c, a
	ld	b, (ix + 0)
	mlt	bc			; left byte * right byte
	ld	a, (iy + 0)		; + the answer's byte
	add	a, c
	ld	c, a
	ld	a, b
	adc	a, 0
	ld	b, a
	ld	a, c			; + the carry
	add	a, l
	ld	(iy + 0), a
	ld	a, b
	adc	a, 0
	ld	l, a			; the carry into the next
	inc	iy
	inc	de
	dec	h
	jr	nz, .llmul_step
	pop	iy
	pop	de
	pop	bc
.llmul_next:
	inc	ix			; the next byte of the left operand
	inc	iy			; lands a byte higher
	dec	c			; and has a step less to go
	jr	nz, .llmul_row

	ld	hl, 8			; the copy, dropped
	add	hl, sp
	ld	sp, hl
	pop	hl
	pop	de
	pop	bc
	pop	iy
	pop	ix
	ret

; iy -> the dividend, which becomes the quotient; de -> the divisor, which is
; overwritten by the remainder. Both unsigned.
;
; Shift and subtract, as the long core is, but with the remainder in
; registers rather than in memory: HL holds its low 24 bits, DE the next 24
; and BC the top 16, and a shift of all three is three add-with-carries. The
; quotient's low 24 bits are in IX, its next 24 on top of the stack and its
; top 16 in the frame. The divisor stays in the frame, where a trial compare
; reads it a limb at a time from the top: most of the time the top limbs
; differ and that settles it, and the subtract is done only when it fits.
;
; The remainder never needs a 65th bit, though it stays below a divisor that
; can be above 2^63: it is what is left of the part of the dividend shifted
; in so far, so it is never larger than that part, which after k rounds is
; k bits. Nothing falls out of the top of BC.
;
; Rounds that cannot subtract are not run. Leading zero bytes of the dividend
; would only shift zeros through a remainder that stays zero; and while the
; remainder is shorter than the divisor it cannot be as large, so a divisor
; of m bytes lets the dividend's top m - 1 bytes go straight into the
; remainder. What is left is eight rounds for each byte of the dividend past
; those: the rounds skipped are exactly shifts, done as a byte move.
;
; A divisor of one byte is the common case -- dividing by ten, or by a power
; of two -- and has a loop of its own. Its remainder fits in A, below 256,
; and needs a ninth bit, which the carry out of the rotate is. That leaves HL
; and DE for the quotient, which with IX holds all of it as 72 bits: the
; dividend sits a byte up in them, so what comes out of the top of DE is its
; top bit, and a round touches no memory at all.
;
; The frame, from iy:
;   iy+0..7    the divisor, and a zero at iy+8 so that its top limb reads
;              as 24 bits; then the remainder, when it is done
;   iy+10..18  the dividend moved up, top byte at iy+18, and the quotient
;              when it is done: at iy+11 from the long loop, iy+10 from the
;              byte one
;   iy+19..26  the remainder, from the dividend's top bytes, and a zero at
;              iy+27 for the same reason as iy+8
;   iy+28      the pushed iy: where the dividend is
;   iy+37      the pushed de: where the divisor is
.lludivmod_core:
	push	bc
	push	de
	push	hl
	push	ix
	push	iy
	ld	hl, -28
	add	hl, sp
	ld	sp, hl
	push	hl
	pop	iy

	ex	de, hl			; the divisor, copied
	lea	de, iy + 0
	ld	bc, 8
	ldir
	xor	a, a			; and zeros from iy+8 to the top
	ld	b, 20
.lldiv_clear:
	ld	(de), a
	inc	de
	djnz	.lldiv_clear

	lea	hl, iy + 7		; the divisor's bytes, from the top
	ld	b, 8
.lldiv_dlen:
	ld	a, (hl)
	or	a, a
	jr	nz, .lldiv_dsize
	dec	hl
	djnz	.lldiv_dlen
	lea	hl, iy + 11		; zero, which C leaves undefined: say
	jp	.lldiv_store		; zero for both rather than loop
.lldiv_dsize:
	dec	b
	ld	c, b			; c = the bytes that go straight over

	ld	hl, (iy + 28)		; the dividend's bytes, from the top
	ld	de, 7
	add	hl, de
	ld	b, 8
.lldiv_nlen:
	ld	a, (hl)
	or	a, a
	jr	nz, .lldiv_move
	dec	hl
	djnz	.lldiv_nlen
	jr	.lldiv_rounds		; zero: nothing to move, and no rounds

.lldiv_move:				; its top byte to iy+18, and c more --
	push	bc			; or b more, when that is fewer: then
	ex	de, hl			; all of it is the remainder, from its
	lea	hl, iy + 18		; bottom byte up
	ld	a, c
	cp	a, b
	jr	c, .lldiv_up
	ld	a, b
.lldiv_up:
	ld	bc, 0
	ld	c, a
	add	hl, bc
	ex	de, hl
	pop	bc
	push	bc
	ld	a, b
	ld	bc, 0
	ld	c, a
	lddr
	pop	bc

.lldiv_rounds:				; eight for each byte past those
	ld	a, b
	sub	a, c
	jr	nc, .lldiv_some
	xor	a, a			; all of it went over: none
.lldiv_some:
	add	a, a
	add	a, a
	add	a, a
	inc	c
	dec	c
	jp	z, .lldiv_byte_divisor

	ld	ix, (iy + 11)		; the quotient's low limb
	ld	hl, (iy + 14)		; and its middle one, kept on the stack
	push	hl
	ld	hl, (iy + 19)		; the remainder, as far as it has come
	ld	de, (iy + 22)
	ld	bc, (iy + 25)
	or	a, a
	jr	z, .lldiv_done

.lldiv_loop:
	add	ix, ix			; {remainder:quotient} <<= 1
	ex	(sp), hl
	adc	hl, hl
	ex	(sp), hl
	rl	(iy + 17)
	rl	(iy + 18)
	adc	hl, hl
	ex	de, hl
	adc	hl, hl
	ex	de, hl
	rl	c
	rl	b

	push	hl			; the top limbs: divisor - remainder
	ld	hl, (iy + 6)
	or	a, a
	sbc	hl, bc
	jr	c, .lldiv_fits_pop	; the remainder's is larger
	jr	nz, .lldiv_no_pop	; the divisor's is
	ld	hl, (iy + 3)		; equal: the middle ones, and the
	sbc	hl, de			; carry is clear
	jr	c, .lldiv_fits_pop
	jr	nz, .lldiv_no_pop
	pop	hl			; equal: the low ones, the other way
	push	bc			; round -- remainder - divisor, and
	ld	bc, (iy + 0)		; add it back, which leaves the carry
	sbc	hl, bc			; the borrow said
	add	hl, bc
	pop	bc
	jr	c, .lldiv_next
	jr	.lldiv_fits
.lldiv_no_pop:
	pop	hl
	jr	.lldiv_next
.lldiv_fits_pop:
	pop	hl
.lldiv_fits:
	inc	ix			; the bit the shift left empty
	push	bc			; remainder -= divisor
	ld	bc, (iy + 0)
	or	a, a
	sbc	hl, bc
	ex	de, hl
	ld	bc, (iy + 3)
	sbc	hl, bc
	ex	de, hl
	ex	(sp), hl		; the top limb, the low one kept
	ld	bc, (iy + 6)		; the zero at iy+8 keeps BC's top
	sbc	hl, bc			; byte zero: only c and b are
	ld	c, l			; taken back
	ld	b, h
	pop	hl
.lldiv_next:
	dec	a
	jr	nz, .lldiv_loop

.lldiv_done:
	ld	(iy + 0), hl		; the remainder, over the divisor
	ld	(iy + 3), de
	ld	(iy + 6), bc
	pop	hl			; the quotient
	ld	(iy + 14), hl
	ld	(iy + 11), ix
	lea	hl, iy + 11
	jr	.lldiv_store

.lldiv_byte_divisor:
	ld	b, a			; the rounds
	ld	c, (iy + 0)		; the divisor
	ld	ix, (iy + 10)		; the quotient, all 72 bits of it
	ld	hl, (iy + 13)
	ld	de, (iy + 16)
	xor	a, a			; the remainder
	or	a, b
	jr	z, .lldiv_byte_done
	xor	a, a
.lldiv_byte:
	add	ix, ix			; {remainder:quotient} <<= 1
	adc	hl, hl
	ex	de, hl
	adc	hl, hl
	ex	de, hl
	rla
	jr	c, .lldiv_byte_fits	; the ninth bit: it fits
	cp	a, c
	jr	c, .lldiv_byte_next
.lldiv_byte_fits:
	sub	a, c
	inc	ix
.lldiv_byte_next:
	djnz	.lldiv_byte

.lldiv_byte_done:
	ld	(iy + 10), ix		; the quotient, a byte down
	ld	(iy + 13), hl
	ld	(iy + 16), de
	lea	hl, iy + 0		; the remainder, over the divisor
	ld	(hl), a
	inc	hl
	xor	a, a
	ld	b, 7
.lldiv_byte_zero:
	ld	(hl), a
	inc	hl
	djnz	.lldiv_byte_zero
	lea	hl, iy + 10

.lldiv_store:				; hl -> the quotient
	ld	de, (iy + 28)
	ld	bc, 8
	ldir
	lea	hl, iy + 0
	ld	de, (iy + 37)
	ld	bc, 8
	ldir

	ld	hl, 28
	add	hl, sp
	ld	sp, hl
	pop	iy
	pop	ix
	pop	hl
	pop	de
	pop	bc
	ret

; The remainder the core left where the divisor was, moved to (iy).
.llrem_move:
	push	bc
	push	de
	push	hl
	ex	de, hl
	lea	de, iy + 0
	ld	bc, 8
	ldir
	pop	hl
	pop	de
	pop	bc
	ret

.llabs_iy:
	bit	7, (iy + 7)
	ret	z
.llneg_iy:
	push	hl
	lea	hl, iy + 0
	call	.llneg_hl
	pop	hl
	ret

.llabs_ix:
	bit	7, (ix + 7)
	ret	z
	push	hl
	lea	hl, ix + 0
	call	.llneg_hl
	pop	hl
	ret

_acc_rt_lldivu:
	push	iy
	push	hl
	pop	iy
	call	.lludivmod_core
	pop	iy
	ret

_acc_rt_llremu:
	push	iy
	push	hl
	pop	iy
	call	.lludivmod_core
	call	.llrem_move
	pop	iy
	ret

; Magnitudes divided and the sign put back, as for a long: the quotient is
; negative when the operands differ in sign, the remainder takes the
; dividend's.
_acc_rt_lldivs:
	push	ix
	push	iy
	push	bc
	push	hl
	pop	iy
	push	de
	pop	ix
	ld	a, (iy + 7)
	xor	a, (ix + 7)
	ld	b, a
	call	.llabs_iy
	call	.llabs_ix
	call	.lludivmod_core
	bit	7, b
	call	nz, .llneg_iy
	pop	bc
	pop	iy
	pop	ix
	ret

_acc_rt_llrems:
	push	ix
	push	iy
	push	bc
	push	hl
	pop	iy
	push	de
	pop	ix
	ld	b, (iy + 7)
	call	.llabs_iy
	call	.llabs_ix
	call	.lludivmod_core
	call	.llrem_move
	bit	7, b
	call	nz, .llneg_iy
	pop	bc
	pop	iy
	pop	ix
	ret

; (hl): eight bytes of integer in, a float out in the first four.
;
; A value that fits in 32 bits is ultof's already. A wider one is shifted
; down until it fits, every bit shifted out kept as a sticky bit in bit 0:
; the float keeps 24 bits and rounds on the eight below them, so a 1 there
; for "something was below" rounds exactly as the whole value would. The
; shift is then added back into the exponent, which cannot overflow: 2^64 is
; far inside a float's range.
_acc_rt_lltof:
	push	iy
	push	hl
	pop	iy
	push	bc
	ld	b, (iy + 7)		; the sign, in bit 7
	bit	7, b
	call	nz, .llneg_hl		; and the magnitude
	jr	.lltof_go
_acc_rt_ulltof:
	push	iy
	push	hl
	pop	iy
	push	bc
	ld	b, 0
.lltof_go:
	ld	c, 0			; how far it was shifted
.lltof_fit:
	ld	a, (iy + 4)
	or	a, (iy + 5)
	or	a, (iy + 6)
	or	a, (iy + 7)
	jr	z, .lltof_small
	srl	(iy + 7)
	rr	(iy + 6)
	rr	(iy + 5)
	rr	(iy + 4)
	rr	(iy + 3)
	rr	(iy + 2)
	rr	(iy + 1)
	rr	(iy + 0)
	jr	nc, .lltof_kept
	set	0, (iy + 0)		; sticky
.lltof_kept:
	inc	c
	jr	.lltof_fit

.lltof_small:
	call	.ultof_entry
	ld	a, c			; exponent += c: c << 23 is c >> 1 in
	or	a, a			; byte 3 and c's low bit at the top of
	jr	z, .lltof_sign		; byte 2
	srl	a
	ld	c, a
	ld	a, 0
	rra
	add	a, (iy + 2)
	ld	(iy + 2), a
	ld	a, c
	adc	a, (iy + 3)
	ld	(iy + 3), a
.lltof_sign:
	ld	a, b
	and	a, 0x80
	or	a, (iy + 3)
	ld	(iy + 3), a
	pop	bc
	pop	iy
	ret

; (hl): a float in the first four bytes in, eight bytes of integer out,
; truncated towards zero. Below 2^31 that is ftol's answer, sign extended.
; From there up the significand is shifted into place -- which is how an
; unsigned long long above 2^63 comes out right too, so one routine serves
; both. Too large for either is undefined, and is left to wrap.
_acc_rt_ftoll:
	push	iy
	push	bc
	push	hl
	pop	iy
	push	hl
	ld	a, (iy + 3)		; the biased exponent
	and	a, 0x7f
	add	a, a
	ld	b, a
	ld	a, (iy + 2)
	rlca
	and	a, 1
	add	a, b
	ld	b, a
	cp	a, 158			; 127 + 31
	jr	nc, .ftoll_big

	call	.ftol_entry
	ld	a, (iy + 3)
	rla
	sbc	a, a
	ld	(iy + 4), a
	ld	(iy + 5), a
	ld	(iy + 6), a
	ld	(iy + 7), a
	jr	.ftoll_out

.ftoll_big:
	ld	c, (iy + 3)		; the sign, in bit 7
	set	7, (iy + 2)		; the leading 1, at bit 23
	xor	a, a
	ld	(iy + 3), a
	ld	(iy + 4), a
	ld	(iy + 5), a
	ld	(iy + 6), a
	ld	(iy + 7), a
	ld	a, b
	sub	a, 150			; the significand goes e - 150 bits
	ld	b, a			; up: at least 8, so never zero
.ftoll_shl:
	sla	(iy + 0)
	rl	(iy + 1)
	rl	(iy + 2)
	rl	(iy + 3)
	rl	(iy + 4)
	rl	(iy + 5)
	rl	(iy + 6)
	rl	(iy + 7)
	djnz	.ftoll_shl
	bit	7, c
	call	nz, .llneg_hl

.ftoll_out:
	pop	hl
	pop	bc
	pop	iy
	ret

