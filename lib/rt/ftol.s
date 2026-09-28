;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
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
