;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_ftol
	XDEF	acc_rt_ftol_entry

	.assume adl=1
	SEGMENT CODE

					; already where rounding looks for them

; hl -> the float, overwritten by the long. Truncated towards zero, as C says;
; a value too large for a long is undefined and is left to wrap.
_acc_rt_ftol:
acc_rt_ftol_entry:				; for the routines that call it
	push	iy
	push	bc
	push	de
	push	hl
	push	hl
	pop	iy			; iy -> the float, and the long after

	ld	a, (iy + 2)		; the biased exponent: its low bit into
	rla				; the carry, and then the byte above it
	ld	a, (iy + 3)
	rla
	ld	b, a
	ld	c, 0			; c = the sign
	jr	nc, .ftol_significand
	inc	c

.ftol_significand:
	; The significand in bits 31..8 of A, D, E and L, top first, which
	; makes the value that / 2^(158 - e).
	ld	l, 0
	ld	e, (iy + 0)
	ld	d, (iy + 1)
	ld	a, (iy + 2)
	or	a, 0x80
	ld	h, a			; the top byte, while A counts

	ld	a, 158			; how far down the point has to come
	sub	a, b
	jr	c, .ftol_store		; past the top: undefined, so as it is
	cp	a, 32
	jr	nc, .ftol_zero		; everything shifts out
.ftol_bytes:
	cp	a, 8			; eight places at a time while there are
	jr	c, .ftol_bits
	ld	l, e
	ld	e, d
	ld	d, h
	ld	h, 0
	sub	a, 8
	jr	.ftol_bytes
.ftol_bits:
	or	a, a
	jr	z, .ftol_store
	ld	b, a
.ftol_shift:
	srl	h			; and then one at a time
	rr	d
	rr	e
	rr	l
	djnz	.ftol_shift
	jr	.ftol_store

.ftol_zero:
	ld	hl, 0
	ld	de, 0

.ftol_store:
	bit	0, c			; the sign, as a subtract from zero
	jr	z, .ftol_out
	xor	a, a
	sub	a, l
	ld	l, a
	ld	a, 0
	sbc	a, e
	ld	e, a
	ld	a, 0
	sbc	a, d
	ld	d, a
	ld	a, 0
	sbc	a, h
	ld	h, a

.ftol_out:
	ld	(iy + 0), l
	ld	(iy + 1), e
	ld	(iy + 2), d
	ld	(iy + 3), h

	pop	hl
	pop	de
	pop	bc
	pop	iy
	ret
