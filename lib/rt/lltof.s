;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lltof
	XDEF	_acc_rt_ulltof
	XREF	acc_rt_llneg_hl
	XREF	acc_rt_ultof_entry

	.assume adl=1
	SEGMENT CODE

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
	call	nz, acc_rt_llneg_hl		; and the magnitude
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
	call	acc_rt_ultof_entry
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
