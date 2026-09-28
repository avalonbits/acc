;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_mul

	.assume adl=1
	SEGMENT CODE

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
