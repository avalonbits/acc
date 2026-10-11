;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_jump8

	.assume adl=1
	SEGMENT CODE

; A switch on a char whose cases lie close together, by a table indexed by
; the value: in A, and after the call the lowest case's value, a byte, the
; number of values from it, a byte, two bytes nothing reads, and that many
; addresses and one more, three bytes each -- a case's, or past the table
; for a value no case has, which the last one is for a value outside them.
; Nothing kept but IX and IY.
_acc_rt_jump8:
	pop	hl			; the table
	sub	a, (hl)			; how far past the lowest
	inc	hl
	cp	a, (hl)
	jr	c, .jump8_in
	ld	a, (hl)			; outside: the one past the cases
.jump8_in:
	inc	hl
	inc	hl
	inc	hl
	ld	de, 0
	ld	e, a
	add	hl, de
	add	hl, de
	add	hl, de
	ld	hl, (hl)
	jp	(hl)
