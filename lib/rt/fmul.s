;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_fmul
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


; The 16-bit product of significand byte i of the left and j of the right,
; added into the column being summed in HL. MLT gives it in B and C, and it
; goes through D and E into DE, whose upper byte is kept zero, so that
; nothing depends on what MLT does to BC's.
	MACRO	FMUL_AT i, j
	ld	b, (ix + (1 + i))
	ld	c, (ix + (5 + j))
	mlt	bc
	ld	d, b
	ld	e, c
	add	hl, de
	ENDMACRO

; The fast path's partial product: left byte i -- 0 and 1 from the operand
; at IX, 2 from the frame with its leading 1 -- times right byte j from the
; frame. With `sum` 0 it is left in B and C for the first column.
	MACRO	FAST_AT i, j, sum
	IF i - 2
	ld	b, (ix + i)
	ELSE
	ld	b, (iy + 3)
	ENDIF
	ld	c, (iy + j)
	mlt	bc
	IF sum
	ld	d, b
	ld	e, c
	add	hl, de
	ENDIF
	ENDMACRO

; One column done: its low byte is product byte k, and the rest carries into
; the next. Storing HL at byte k and loading it back from k + 1 does both --
; the load reads the carry as H and U, and a zero from the byte above, which
; no column has written yet.
	MACRO	FMUL_CARRY k
	ld	(ix + (16 + k)), hl
	ld	hl, (ix + (17 + k))
	ENDMACRO

_acc_rt_fmul:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl

	; ---------------------------------------------------- the fast path
	; Two normal numbers whose product is normal, which is nearly every
	; multiply a program does, worked straight from the operands: the
	; exponents and sign from their top bytes, the product on a small frame
	; at IY, and the rounding and packing in registers. Anything else --
	; a zero, a denormal, an infinity or a NaN on either side, or a product
	; past either end of the exponents -- goes to the general code below,
	; which unpacks both into its own frame and handles every case. Nothing
	; is written to the destination until the answer is known to be normal.
	;
	; The frame: 0..2 the right significand with its leading 1, 3 the left's
	; top byte with its leading 1, 4..9 the product (4 and 5 only as far as
	; the rounding needs them), 10 a zero for the last carry, 11..13 the
	; exponent, 14 the sign.
	push	hl
	pop	ix			; the left operand, and the destination
	push	de
	pop	iy			; the right, until the frame takes IY

	ld	a, (ix + 2)		; the exponent's low bit into the carry,
	rla				; and then the byte above it: A is the
	ld	a, (ix + 3)		; biased exponent
	rla
	ld	c, a
	dec	a			; 1 to 254 is a normal number
	cp	a, 254
	jp	nc, .fmul_general
	ld	a, (iy + 2)
	rla
	ld	a, (iy + 3)
	rla
	ld	b, a
	dec	a
	cp	a, 254
	jp	nc, .fmul_general

	ld	hl, -127		; the exponent, one bias taken back off;
	ld	de, 0			; one more if the product reaches 2
	ld	e, c
	add	hl, de
	ld	e, b
	add	hl, de
	ld	a, (ix + 3)		; the sign is the two signs differing
	xor	a, (iy + 3)
	and	a, 0x80
	ld	c, a

	ex	de, hl
	ld	hl, -15
	add	hl, sp
	ld	sp, hl
	ld	a, (iy + 0)		; the right significand, and the left's
	ld	(hl), a			; top byte, with their leading 1s
	inc	hl
	ld	a, (iy + 1)
	ld	(hl), a
	inc	hl
	ld	a, (iy + 2)
	or	a, 0x80
	ld	(hl), a
	inc	hl
	ld	a, (ix + 2)
	or	a, 0x80
	ld	(hl), a
	ld	iy, 0
	add	iy, sp
	ld	(iy + 11), de
	ld	(iy + 14), c
	ld	hl, 0
	ld	(iy + 5), hl		; bytes 5 to 10 zero, for the carries
	ld	(iy + 8), hl
	ld	de, 0			; and DE's upper byte, for FAST_AT

	FAST_AT	0, 0, 0
	ld	h, b
	ld	l, c
	ld	(iy + 4), hl
	ld	hl, (iy + 5)
	FAST_AT	1, 0, 1
	FAST_AT	0, 1, 1
	ld	(iy + 5), hl
	ld	hl, (iy + 6)
	FAST_AT	2, 0, 1
	FAST_AT	1, 1, 1
	FAST_AT	0, 2, 1
	ld	(iy + 6), hl
	ld	hl, (iy + 7)
	FAST_AT	2, 1, 1
	FAST_AT	1, 2, 1
	ld	(iy + 7), hl
	ld	hl, (iy + 8)
	FAST_AT	2, 2, 1			; H and L are the product's top two bytes

	; The significand is the top twenty-four bits in H, L and C, and A the
	; eight below them, with anything further down kept in its lowest bit.
	ld	c, (iy + 7)
	ld	a, (iy + 5)
	or	a, (iy + 4)
	ld	a, (iy + 6)
	jr	z, .fmul_fast_sticky
	or	a, 1
.fmul_fast_sticky:
	ld	de, (iy + 11)
	bit	7, h			; at bit 47: between 2 and 4, one more
	jr	nz, .fmul_fast_top	; exponent
	sla	a			; below it: one place up
	rl	c
	rl	l
	rl	h
	jr	.fmul_fast_round
.fmul_fast_top:
	inc	de
.fmul_fast_round:
	ld	b, (iy + 14)		; the sign, and the rest is fadd's
	call	acc_rt_fround
	jr	c, .fmul_fast_undo

	ld	hl, 15
	add	hl, sp
	ld	sp, hl
	pop	hl
	pop	de
	pop	bc
	pop	iy
	pop	ix
	ret

.fmul_fast_undo:
	ld	hl, 15
	add	hl, sp
	ld	sp, hl

	; ------------------------------------------------- the general case
.fmul_general:
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
	jp	z, .fmul_left_special
	ld	a, (ix + 9)
	cp	a, 255
	jp	z, .fmul_right_special

	ld	a, (ix + 8)		; either operand zero makes the product
	or	a, a			; zero, which keeps the sign the two
	jp	z, acc_rt_fadd_zero		; operands gave it
	ld	a, (ix + 9)
	or	a, a
	jp	z, acc_rt_fadd_zero

	; A denormal comes out of acc_rt_funpack with its leading bit clear, and the
	; product below expects both leading bits at the top: shifted up here,
	; with the places it took coming off the exponent.
	push	ix
	pop	iy
	call	acc_rt_fnorm
	ld	(ix + 23), a
	lea	iy, ix + 4
	call	acc_rt_fnorm
	add	a, (ix + 23)
	ld	(ix + 23), a

	; The product, a column at a time: column k is the sum of the partial
	; products whose bytes add to k, at most three of them, which fits in
	; HL with the carry from below.
	ld	hl, 0
	ld	(ix + 17), hl		; bytes 17 to 22 zero, for the carries
	ld	(ix + 20), hl
	ld	de, 0			; and DE's upper byte, for FMUL_AT

	FMUL_AT 0, 0
	FMUL_CARRY 0
	FMUL_AT 1, 0
	FMUL_AT 0, 1
	FMUL_CARRY 1
	FMUL_AT 2, 0
	FMUL_AT 1, 1
	FMUL_AT 0, 2
	FMUL_CARRY 2
	FMUL_AT 2, 1
	FMUL_AT 1, 2
	FMUL_CARRY 3
	FMUL_AT 2, 2
	ld	(ix + 20), hl		; bytes 4 and 5, and a zero above

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
	ld	e, (ix + 23)		; and the places a denormal was moved
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
	jp	z, acc_rt_fadd_subnormal	; zero and below is a denormal or nothing
	cp	a, 255
	jp	nc, acc_rt_fadd_infinity
	ld	(ix + 8), e
	jp	acc_rt_fadd_round

.fmul_wide_exponent:
	cp	a, 0xff			; the high byte says which end it ran off
	jp	z, acc_rt_fadd_subnormal
	jp	acc_rt_fadd_infinity

.fmul_left_special:
	ld	a, (ix + 0)
	or	a, (ix + 1)
	or	a, (ix + 2)
	or	a, (ix + 3)
	jp	nz, acc_rt_fadd_nan

	ld	a, (ix + 9)		; an infinity times zero is the one
	or	a, a			; product with no answer: the two pull
	jp	z, acc_rt_fadd_nan		; in opposite directions
	cp	a, 255
	jp	nz, acc_rt_fadd_infinity
	ld	a, (ix + 4)		; the right side special as well
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	jp	nz, acc_rt_fadd_nan
	jp	acc_rt_fadd_infinity		; two infinities multiply to one

.fmul_right_special:
	ld	a, (ix + 4)
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	jp	nz, acc_rt_fadd_nan
	ld	a, (ix + 8)
	or	a, a
	jp	z, acc_rt_fadd_nan		; zero times an infinity
	jp	acc_rt_fadd_infinity
