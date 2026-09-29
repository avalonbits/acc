;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_fdiv
	XREF	acc_rt_fadd_infinity
	XREF	acc_rt_fadd_nan
	XREF	acc_rt_fadd_round
	XREF	acc_rt_fadd_subnormal
	XREF	acc_rt_fadd_zero
	XREF	acc_rt_fround
	XREF	acc_rt_fnorm
	XREF	acc_rt_funpack

	.assume adl=1
	SEGMENT CODE

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


_acc_rt_fdiv:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl

	; ---------------------------------------------------- the fast path
	; Two normal numbers whose quotient is normal, as fmul's: the exponents
	; and sign from the top bytes, both significands with their leading 1s
	; on a small frame at IY, the division in registers, and the rounding
	; and packing in acc_rt_fround. Anything else goes to the general code
	; below, and nothing is written until the answer is known to be normal.
	; The frame: 0..2 the dividend's significand, 3..5 the divisor's, 6..8
	; the exponent, 9 the sign.
	push	hl
	pop	ix			; the dividend, and the destination
	push	de
	pop	iy			; the divisor, until the frame takes IY

	ld	a, (ix + 2)		; the biased exponents, as fmul reads them
	rla
	ld	a, (ix + 3)
	rla
	ld	c, a
	dec	a
	cp	a, 254
	jp	nc, .fdiv_general
	ld	a, (iy + 2)
	rla
	ld	a, (iy + 3)
	rla
	ld	b, a
	dec	a
	cp	a, 254
	jp	nc, .fdiv_general

	ld	hl, 127			; the exponent: the difference, and the
	ld	de, 0			; bias; one less if the quotient's leading
	ld	e, c			; bit comes out below bit 31
	add	hl, de
	ld	e, b
	or	a, a
	sbc	hl, de
	ld	a, (ix + 3)		; the sign is the two signs differing
	xor	a, (iy + 3)
	and	a, 0x80
	ld	c, a

	ex	de, hl
	ld	hl, -10
	add	hl, sp
	ld	sp, hl
	ld	a, (ix + 0)		; both significands, with their leading 1s
	ld	(hl), a
	inc	hl
	ld	a, (ix + 1)
	ld	(hl), a
	inc	hl
	ld	a, (ix + 2)
	or	a, 0x80
	ld	(hl), a
	inc	hl
	ld	a, (iy + 0)
	ld	(hl), a
	inc	hl
	ld	a, (iy + 1)
	ld	(hl), a
	inc	hl
	ld	a, (iy + 2)
	or	a, 0x80
	ld	(hl), a
	ld	iy, 0
	add	iy, sp
	ld	(iy + 6), de
	ld	(iy + 9), c

	; The quotient, thirty-two bits, as the general code's loop makes it,
	; except that each finished byte is pushed: popped again below, they
	; come off lowest first.
	ld	hl, (iy + 0)
	ld	de, (iy + 3)
	ld	c, 4
	or	a, a
.fdiv_fast_byte:
	ld	b, 8
.fdiv_fast_bit:
	jr	c, .fdiv_fast_force
	or	a, a
	sbc	hl, de
	jr	nc, .fdiv_fast_one
	add	hl, de
	ccf
	jr	.fdiv_fast_take
.fdiv_fast_force:
	or	a, a
	sbc	hl, de
.fdiv_fast_one:
	scf
.fdiv_fast_take:
	rla
	add	hl, hl
	djnz	.fdiv_fast_bit
	push	af			; the byte, and the carry with it
	dec	c
	jr	nz, .fdiv_fast_byte

	; What is left of the remainder, the last doubling's carry included,
	; is the sticky bit, in C.
	jr	c, .fdiv_fast_sticky
	ld	de, 0
	or	a, a
	sbc	hl, de
	jr	z, .fdiv_fast_exact
.fdiv_fast_sticky:
	inc	c
.fdiv_fast_exact:
	pop	af			; the guard byte, with the sticky in it
	or	a, c
	ld	b, a
	pop	af
	ld	c, a
	pop	af
	ld	l, a
	pop	af
	ld	h, a
	ld	a, b
	ld	de, (iy + 6)
	bit	7, h			; a leading bit below 31: one place up,
	jr	nz, .fdiv_fast_round	; and one exponent less
	sla	a
	rl	c
	rl	l
	rl	h
	dec	de
.fdiv_fast_round:
	ld	b, (iy + 9)
	call	acc_rt_fround
	jr	c, .fdiv_fast_undo

	ld	hl, 10
	add	hl, sp
	ld	sp, hl
	pop	hl
	pop	de
	pop	bc
	pop	iy
	pop	ix
	ret

.fdiv_fast_undo:
	ld	hl, 10
	add	hl, sp
	ld	sp, hl

	; ------------------------------------------------- the general case
.fdiv_general:
	pop	hl			; the operands' addresses again, from
	pop	de			; where they were saved
	push	de
	push	hl

	ld	ix, -24
	add	ix, sp
	ld	sp, ix
	ld	(ix + 12), hl

	push	ix
	pop	iy
	call	acc_rt_funpack
	ld	(ix + 8), b
	ld	(ix + 10), c

	push	ix
	pop	iy
	ld	bc, 4
	add	iy, bc
	ex	de, hl
	call	acc_rt_funpack
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
	jp	z, acc_rt_fadd_nan
	jp	acc_rt_fadd_zero

.fdiv_nonzero:
	ld	a, (ix + 9)		; a finite number over zero is an
	or	a, a			; infinity, which is what IEEE says and
	jp	z, acc_rt_fadd_infinity	; C leaves undefined

	jp	.fdiv_finite

.fdiv_left_special:
	ld	a, (ix + 0)
	or	a, (ix + 1)
	or	a, (ix + 2)
	or	a, (ix + 3)
	jp	nz, acc_rt_fadd_nan

	ld	a, (ix + 9)		; an infinity over an infinity has no
	cp	a, 255			; answer; over anything else it is an
	jp	nz, acc_rt_fadd_infinity	; infinity
	ld	a, (ix + 4)
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	jp	nz, acc_rt_fadd_nan
	jp	acc_rt_fadd_nan

.fdiv_right_special:
	ld	a, (ix + 4)
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	jp	nz, acc_rt_fadd_nan
	jp	acc_rt_fadd_zero		; a finite number over an infinity

.fdiv_finite:
	; Denormals shifted up to a leading bit, as in fmul: the dividend's
	; places come off the exponent and the divisor's go on.
	push	ix
	pop	iy
	call	acc_rt_fnorm
	ld	(ix + 15), a
	lea	iy, ix + 4
	call	acc_rt_fnorm
	ld	(ix + 23), a

	; The exponent: the difference of the two, the bias, and the places
	; the denormals were moved. One less if the quotient's leading bit
	; comes out below bit 31, which is known after the loop.
	ld	hl, 0
	ld	l, (ix + 8)
	ld	de, 0
	ld	e, (ix + 9)
	or	a, a
	sbc	hl, de			; the exponents' difference, which may
	ld	de, 127			; have gone negative, plus the bias
	add	hl, de
	ld	de, 0
	ld	e, (ix + 23)		; plus the divisor's shift
	add	hl, de
	ld	e, (ix + 15)		; less the dividend's
	or	a, a
	sbc	hl, de
	ld	(ix + 20), hl		; kept until there is a quotient to go
					; with it, because whether it is in
					; range decides nothing until then

	; The quotient, thirty-two bits of it, from bit 31 down: each step
	; asks whether the divisor fits the remainder, takes it off if it
	; does, and doubles what is left. Bit 31 is the dividend against the
	; divisor as they stand, so it is 1 when the dividend's significand is
	; the larger and the ratio is in [1, 2), and 0 when it is in [1/2, 1).
	; The bits are gathered in A and stored a byte at a time, top byte
	; first, from ix+3 down to ix+0, where the guard byte is.
	;
	; Doubling can carry out of twenty-four bits, and that carry means the
	; divisor fits whatever the borrow says.
	ld	hl, (ix + 1)		; the dividend's significand
	ld	de, (ix + 5)		; and the divisor's, for the whole loop
	lea	iy, ix + 3
	ld	c, 4
	or	a, a			; no carry into the first step
.fdiv_byte:
	ld	b, 8
.fdiv_bit:
	jr	c, .fdiv_force
	or	a, a
	sbc	hl, de
	jr	nc, .fdiv_one
	add	hl, de			; it did not fit: put it back, and a 0
	ccf				; (the add carried)
	jr	.fdiv_take
.fdiv_force:
	or	a, a			; above twenty-four bits, so the divisor
	sbc	hl, de			; fits however the borrow reads
.fdiv_one:
	scf
.fdiv_take:
	rla				; the bit into the byte being gathered
	add	hl, hl			; and the remainder doubles
	djnz	.fdiv_bit		; (none of these three touch the carry)
	ld	(iy + 0), a
	dec	iy
	dec	c
	jr	nz, .fdiv_byte

	; A leading bit below 31: one place up, and one exponent less.
	jr	nc, .fdiv_sticky	; the last doubling's carry is part of the
	ld	hl, 1			; remainder: anything there is sticky
.fdiv_sticky:
	bit	7, (ix + 3)
	jr	nz, .fdiv_placed
	sla	(ix + 0)
	rl	(ix + 1)
	rl	(ix + 2)
	rl	(ix + 3)
	push	hl
	ld	hl, (ix + 20)
	dec	hl
	ld	(ix + 20), hl
	pop	hl
.fdiv_placed:

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
	jp	z, acc_rt_fadd_subnormal
	cp	a, 255
	jp	nc, acc_rt_fadd_infinity
	ld	(ix + 8), e
	jp	acc_rt_fadd_round

.fdiv_wide_exponent:
	cp	a, 0xff			; the high byte says which end it ran off
	jp	z, acc_rt_fadd_subnormal
	jp	acc_rt_fadd_infinity
