;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_llshrs
	XDEF	_acc_rt_llshru

	.assume adl=1
	SEGMENT CODE

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
