;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_ltof
	XDEF	_acc_rt_ultof
	XDEF	acc_rt_ultof_entry
	XREF	acc_rt_fround

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
; below it in bits 7..0, which is exactly the shape acc_rt_fround takes.


_acc_rt_ltof:
	push	ix
	push	bc
	push	de
	push	hl
	call	.ltof_load
	ld	b, 0			; the sign
	bit	7, h
	jr	z, .ltof_magnitude
	ld	b, 0x80			; negative: the magnitude, from zero
	xor	a, a
	sub	a, e
	ld	e, a
	ld	a, 0
	sbc	a, c
	ld	c, a
	ld	a, 0
	sbc	a, l
	ld	l, a
	ld	a, 0
	sbc	a, h
	ld	h, a
	jr	.ltof_magnitude

_acc_rt_ultof:
acc_rt_ultof_entry:
	push	ix
	push	bc
	push	de
	push	hl
	call	.ltof_load
	ld	b, 0			; never negative, so no magnitude to take

.ltof_magnitude:
	; The magnitude moved up until its leading 1 is at bit 31: a whole
	; byte at a time while the top one is empty, then a bit at a time.
	; D counts the exponent down from 158, 127 + 31.
	ld	d, 158
	ld	a, h
	or	a, l
	or	a, c
	or	a, e
	jr	z, .ltof_zero
.ltof_bytes:
	ld	a, h
	or	a, a
	jr	nz, .ltof_bits
	ld	h, l
	ld	l, c
	ld	c, e
	ld	e, 0
	ld	a, d
	sub	a, 8
	ld	d, a
	jr	.ltof_bytes
.ltof_bits:
	bit	7, h
	jr	nz, .ltof_round
	sla	e
	rl	c
	rl	l
	rl	h
	dec	d
	jr	.ltof_bits

.ltof_round:
	; The top three bytes are the significand and the lowest the eight
	; below it, which is what acc_rt_fround takes; the exponent is always
	; in range, so it always writes.
	ld	a, e
	ld	e, d
	ld	d, 0
	call	acc_rt_fround
	jr	.ltof_done

.ltof_zero:
	ld	(ix + 0), a		; a zero long is +0
	ld	(ix + 1), a
	ld	(ix + 2), a
	ld	(ix + 3), a

.ltof_done:
	pop	hl
	pop	de
	pop	bc
	pop	ix
	ret

; The long at (hl) into H, L, C and E, top first, with IX at it for the
; float that replaces it.
.ltof_load:
	push	hl
	pop	ix
	ld	e, (ix + 0)
	ld	c, (ix + 1)
	ld	l, (ix + 2)
	ld	h, (ix + 3)
	ret
