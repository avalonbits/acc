;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_ltof
	XDEF	_acc_rt_ultof
	XDEF	acc_rt_ultof_entry
	XREF	acc_rt_fadd_round
	XREF	acc_rt_fadd_zero

	.assume adl=1
	SEGMENT CODE

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
acc_rt_ultof_entry:
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
	jp	z, acc_rt_fadd_zero

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
	jp	acc_rt_fadd_round		; the eight bits below the number are
