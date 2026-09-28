;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_fmul
	XREF	acc_rt_fadd_infinity
	XREF	acc_rt_fadd_nan
	XREF	acc_rt_fadd_round
	XREF	acc_rt_fadd_subnormal
	XREF	acc_rt_fadd_zero
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
; added into the running product at byte k and carried up to the top.
	MACRO	FMUL_AT i, j, k
	ld	b, (ix + (1 + i))
	ld	c, (ix + (5 + j))
	mlt	bc
	ld	a, (ix + (16 + k))
	add	a, c
	ld	(ix + (16 + k)), a
	ld	a, (ix + (17 + k))
	adc	a, b
	ld	(ix + (17 + k)), a
	IF 1 - (k >> 2)
	ld	a, (ix + (18 + k))
	adc	a, 0
	ld	(ix + (18 + k)), a
	ENDIF
	IF 1 - ((k + 1) >> 2)
	ld	a, (ix + (19 + k))
	adc	a, 0
	ld	(ix + (19 + k)), a
	ENDIF
	IF 1 - ((k + 2) >> 2)
	ld	a, (ix + (20 + k))
	adc	a, 0
	ld	(ix + (20 + k)), a
	ENDIF
	IF 1 - ((k + 3) >> 2)
	ld	a, (ix + (21 + k))
	adc	a, 0
	ld	(ix + (21 + k)), a
	ENDIF
	ENDMACRO

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
	ld	(ix + 22), a
	lea	iy, ix + 4
	call	acc_rt_fnorm
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
